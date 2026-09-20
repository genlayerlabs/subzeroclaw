"""Decision controller against HTTP fixtures and real shell commands, no API spend."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BIN = Path(__file__).resolve().parents[1] / 'subzeroclaw'


def decision_reply(body, choice=None):
    answers = {}
    for name, q in body['questions'].items():
        labels = list(q['criteria'])
        pick = choice or ('archive' if 'archive' in labels else
                          next((x for x in labels if x.startswith('action_')),
                               'finish' if 'finish' in labels else 'generate'))
        answers[name] = {'type': 'choice', 'choice': pick, 'confidence': .99,
                         'probabilities': {label: float(label == pick) for label in labels}}
    return {'model': 'fixture-decision', 'answers': answers, 'usage': {'input_tokens': 100, 'cost': .00001}}


class DecisionLoopTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='szc-decisions-')
        self.addCleanup(self.tmp.cleanup)
        self.home = Path(self.tmp.name)
        self.skills = self.home / '.subzeroclaw/skills'
        self.skills.mkdir(parents=True)
        self.config = self.home / '.subzeroclaw/config'
        self.config.write_text('max_turns=30\ndecision_context_bytes=4096\n')
        self.calls = []
        self.headers = []
        self.decide = decision_reply
        self.generate = lambda body: {'actions': [], 'answer': 'done'}
        case = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                case.calls.append((self.path, body))
                case.headers.append(dict(self.headers))
                if self.path == '/v1/decisions':
                    data = case.decide(body)
                else:
                    data = {'choices': [{'finish_reason': 'stop', 'message': {
                        'role': 'assistant', 'content': json.dumps(case.generate(body))}}]}
                data = json.dumps(data).encode()
                self.send_response(200)
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)

    def run_agent(self, prompt='complete the fixture task', extra=None):
        env = {'HOME': str(self.home), 'PATH': os.environ['PATH'],
               'SUBZEROCLAW_API_KEY': 'local-fixture-only',
               'SUBZEROCLAW_ENDPOINT': f'http://127.0.0.1:{self.server.server_port}/v1/chat/completions',
               'SUBZEROCLAW_DECISION_EXTRA': '{"policy_ir":["decision-fixture"]}',
               'SUBZEROCLAW_REQUEST_EXTRA': json.dumps(extra or {'model': 'generation-fixture'})}
        return subprocess.run([str(BIN), prompt], env=env, cwd=self.home, capture_output=True, timeout=15)

    def archive(self):
        return [json.loads(line) for p in (self.home / '.subzeroclaw/logs').glob('*.events.jsonl')
                for line in p.read_text().splitlines()]

    def test_multiple_tools_between_generation_and_verified_completion(self):
        self.generate = lambda _: {'actions': [
            {'description': 'create result', 'command': 'printf evidence > result.txt'},
            {'description': 'inspect result', 'command': 'cat result.txt', 'after': [0]},
            {'description': 'verify requested result', 'command': "test \"$(cat result.txt)\" = evidence", 'after': [1], 'verify': True},
        ], 'answer': 'verified result'}
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b'verified result', result.stdout)
        chats = [b for p, b in self.calls if p == '/v1/chat/completions']
        self.assertEqual(len(chats), 1)
        self.assertNotIn('tools', chats[0])
        options = [b['questions']['next']['criteria'] for p, b in self.calls if 'next' in b.get('questions', {})]
        self.assertNotIn('action_1', options[1])
        self.assertNotIn('finish', options[1])
        self.assertIn('finish', options[-1])
        records = self.archive()
        ids = [c['id'] for m in records for c in m.get('tool_calls', [])]
        self.assertEqual(ids, [m['tool_call_id'] for m in records if m['role'] == 'tool'])
        self.assertTrue(all(h.get('X-Unhardcoded-Session') for h in self.headers))

    def test_bootstrap_and_discovery_create_actions_without_generation(self):
        dynamic = [{'description': 'read discovered file', 'command': 'cat observed.txt'}]
        (self.home / 'observed.txt').write_text('discovered evidence')
        (self.skills / 'actions.json').write_text(json.dumps([{
            'description': 'discover files', 'command': 'printf %s ' + shlex.quote(json.dumps(dynamic)), 'discover': True}]))
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        paths = [p for p, _ in self.calls]
        self.assertEqual(paths[:3], ['/v1/decisions'] * 3)
        chat = next(b for p, b in self.calls if p == '/v1/chat/completions')
        self.assertIn('discovered evidence', str(chat['messages']))
        self.assertEqual(paths.count('/v1/chat/completions'), 1)

    def test_failed_action_blocks_dependencies_and_requires_new_generation(self):
        count = 0
        def generate(_):
            nonlocal count
            count += 1
            if count == 1:
                return {'actions': [
                    {'description': 'failing action', 'command': 'exit 7'},
                    {'description': 'must not run', 'command': 'touch forbidden', 'after': [0], 'verify': True},
                ], 'answer': 'wrong success'}
            return {'actions': [{'description': 'verify failure recovery', 'command': 'test ! -e forbidden', 'verify': True}], 'answer': 'recovered'}
        self.generate = generate
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(count, 2)
        self.assertFalse((self.home / 'forbidden').exists())
        self.assertNotIn(b'wrong success', result.stdout)

    def test_invalid_decision_never_executes_an_action(self):
        (self.skills / 'actions.json').write_text('[{"description":"fixture","command":"touch forbidden"}]')
        self.decide = lambda body: decision_reply(body, 'invented_command')
        result = self.run_agent()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.home / 'forbidden').exists())
        self.assertEqual(len(self.calls), 1)

    def test_invalid_plan_does_not_execute_embedded_commands(self):
        self.config.write_text('max_turns=3\n')
        self.generate = lambda _: {'actions': [{'description': 'bad dependency', 'command': 'touch forbidden', 'after': [0]}]}
        result = self.run_agent()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.home / 'forbidden').exists())

    def test_memory_selection_preserves_instructions_and_full_archive(self):
        self.generate = lambda _: {'actions': [
            {'description': f'observe evidence {i}', 'command': "printf '" + f'EVIDENCE_{i}_' + 'x' * 1900 + "'",
             'after': [i-1] if i else [], 'verify': i == 7}
            for i in range(8)], 'answer': 'all observations collected'}
        result = self.run_agent('Keep this user constraint exactly.')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        compactions = [b for _, b in self.calls if 'next' not in b.get('questions', {}) and 'questions' in b]
        self.assertTrue(compactions)
        for body in compactions:
            self.assertIn('Keep this user constraint exactly.', str(body['state']['instructions']))
        records = self.archive()
        self.assertEqual(sum(m['role'] == 'tool' for m in records), 8)
        self.assertIn('EVIDENCE_0_' + 'x' * 1900, str(records))
        self.assertTrue(all(len(json.dumps(b, separators=(',', ':')).encode()) <= 32000
                            for p, b in self.calls if p == '/v1/decisions'))

    def test_oversized_pinned_instructions_fail_before_provider_spend(self):
        (self.skills / 'system.md').write_text('Pinned instruction. ' * 3000)
        result = self.run_agent()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.calls)
        self.assertTrue(self.archive())

    def test_invalid_memory_selection_keeps_the_active_history(self):
        self.generate = lambda _: {'actions': [
            {'description': f'observe {i}', 'command': "printf '" + f'ORIGINAL_{i}_' + 'x' * 1700 + "'",
             'after': [i-1] if i else [], 'verify': i == 4}
            for i in range(5)], 'answer': 'done'}
        self.decide = lambda b: decision_reply(b) if 'next' in b['questions'] else {'answers': {}}
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        last = [b for _, b in self.calls if 'next' in b.get('questions', {})][-1]
        self.assertIn('ORIGINAL_0_', str(last['state']['history']))

    def test_generation_flow_is_forwarded_without_client_model_selection(self):
        flow = ['flow', {'fixture': 'router owns routing'}]
        result = self.run_agent(extra={'flow_ir': flow})
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        chat = next(b for p, b in self.calls if p == '/v1/chat/completions')
        self.assertEqual(chat['flow_ir'], flow)
        self.assertNotIn('model', chat)
        self.assertTrue(all('flow_ir' not in b for p, b in self.calls if p == '/v1/decisions'))

    def test_provider_configuration_is_scrubbed_from_selected_shell(self):
        (self.skills / 'actions.json').write_text(json.dumps([{'description': 'verify scrubbed environment',
            'command': 'test -z "$SUBZEROCLAW_DECISION_EXTRA$SUBZEROCLAW_API_KEY$SUBZEROCLAW_REQUEST_EXTRA"', 'verify': True}]))
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertTrue(any(m['role'] == 'tool' and '[exit:0]' in m['content'] for m in self.archive()))


if __name__ == '__main__':
    unittest.main()
