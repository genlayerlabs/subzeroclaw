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
        pick = labels[0] if name != 'next' and labels[0].startswith('value_') else choice or ('archive' if 'archive' in labels else
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
        self.finish_reason = 'stop'
        self.signal_compact = False
        self.generated_tool_calls = None
        self.compact = lambda body: {'messages': body['messages']}
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
                elif self.path == '/v1/compact':
                    data = case.compact(body)
                else:
                    plan = case.generate(body)
                    data = {'choices': [{'finish_reason': case.finish_reason, 'message': {
                        'role': 'assistant', 'content': plan if isinstance(plan, str) else json.dumps(plan)}}]}
                    if case.signal_compact:
                        data['x_router'] = {'compact': True}
                    if case.generated_tool_calls:
                        data['choices'][0]['message']['tool_calls'] = case.generated_tool_calls
                status = 200
                if isinstance(data, tuple):
                    status, data = data
                data = json.dumps(data).encode()
                self.send_response(status)
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)

    def run_agent(self, prompt='complete the fixture task', extra=None, economy=None):
        env = {'HOME': str(self.home), 'PATH': os.environ['PATH'],
               'SUBZEROCLAW_API_KEY': 'local-fixture-only',
               'SUBZEROCLAW_ENDPOINT': f'http://127.0.0.1:{self.server.server_port}/v1/chat/completions',
               'SUBZEROCLAW_DECISION_EXTRA': '{"policy_ir":["decision-fixture"]}',
               'SUBZEROCLAW_REQUEST_EXTRA': json.dumps(extra or {'model': 'generation-fixture'})}
        if economy is not None:
            env['SUBZEROCLAW_ECONOMY_EXTRA'] = json.dumps(economy)
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
        self.assertNotIn('action_1', options[0])
        self.assertNotIn('finish', options[0])
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
        self.assertEqual(paths[:3], ['/v1/decisions', '/v1/decisions', '/v1/chat/completions'])
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

    def test_transient_decision_failure_does_not_repeat_a_tool(self):
        (self.skills / 'actions.json').write_text(json.dumps([
            {'description': 'record one execution', 'command': 'printf x >> executed'},
            {'description': 'verify once', 'command': 'test "$(cat executed)" = x', 'after': [0], 'verify': True}]))
        failed = False
        def decide(body):
            nonlocal failed
            actions = body['state']['actions']
            if actions and actions[0]['status'] == 1 and not failed:
                failed = True
                return 502, {'error': {'message': 'temporary gateway failure'}}
            return decision_reply(body)
        self.decide = decide
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertTrue(failed)
        self.assertEqual((self.home / 'executed').read_text(), 'x')
        decisions = [b for p, b in self.calls if p == '/v1/decisions']
        self.assertEqual(decisions[1], decisions[2])

    def test_permanent_http_error_is_not_retried(self):
        (self.skills / 'actions.json').write_text('[{"description":"fixture","command":"true"}]')
        self.decide = lambda _: (401, {'error': {'message': 'invalid credential'}})
        result = self.run_agent()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(len(self.calls), 1)

    def test_persistent_gateway_failure_has_bounded_retries(self):
        (self.skills / 'actions.json').write_text('[{"description":"fixture","command":"true"}]')
        self.decide = lambda _: (503, {'error': {'message': 'unavailable'}})
        result = self.run_agent()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(len(self.calls), 3)
        self.assertTrue(all(b == self.calls[0][1] for _, b in self.calls))

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

    def test_memory_selection_reaches_units_after_a_retained_batch(self):
        (self.skills / 'actions.json').write_text(json.dumps([
            {'description': f'observe {i}', 'command': "printf '" + f'OBS_{i}_' + 'x' * 500 + "'",
             'after': [i-1] if i else []} for i in range(12)]))

        def decide(body):
            if 'next' in body['questions']:
                return decision_reply(body)
            reply = decision_reply(body, 'keep')
            for unit in body['state']['candidates']:
                if any(f'OBS_{i}_' in str(unit['messages']) for i in range(8, 12)):
                    key = unit['id']
                    reply['answers'][key] = decision_reply(body, 'archive')['answers'][key]
            return reply

        self.decide = decide
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        last = [b for _, b in self.calls if 'next' in b.get('questions', {})][-1]
        history = str(last['state']['history'])
        for i in range(8):
            self.assertIn(f'OBS_{i}_', history)
        self.assertNotIn('OBS_8_', history)
        self.assertNotIn('OBS_9_', history)
        self.assertEqual(sum(m['role'] == 'tool' for m in self.archive()), 12)
        self.assertIn('OBS_8_' + 'x' * 500, str(self.archive()))

    def test_memory_selection_resumes_after_removing_a_batch(self):
        (self.skills / 'actions.json').write_text(json.dumps([
            {'description': f'observe {i}', 'command': "printf '" + f'OBS_{i}_' + 'x' * 800 + "'",
             'after': [i-1] if i else []} for i in range(12)]))

        def decide(body):
            if 'next' in body['questions']:
                return decision_reply(body)
            done = all(a['status'] == 1 for a in body['state']['actions'])
            return decision_reply(body, 'archive' if done else 'keep')

        self.decide = decide
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        # Generation is forced when all actions are done. Its proposal is not
        # added to decision history, so the final decision sees the selected view.
        completed = [b for _, b in self.calls if 'next' in b.get('questions', {})][-1]
        self.assertNotIn('OBS_8_', str(completed['state']['history']))
        self.assertIn('OBS_11_', str(completed['state']['history']))
        self.assertEqual(sum(m['role'] == 'tool' for m in self.archive()), 12)

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

    def test_plan_commands_are_archived_without_duplication_in_generation_history(self):
        marker = 'PREPARED_COMMAND_' + 'x' * 1200
        generations = []
        def generate(body):
            generations.append(body)
            if len(generations) == 1:
                return {'actions': [{'description': 'write and verify artifact',
                    'command': "printf %s '" + marker + "' > artifact && test -s artifact", 'verify': True}], 'answer': None}
            return {'actions': [], 'answer': 'verified'}
        self.generate = generate
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(len(generations), 2)
        history = generations[-1]['messages']
        self.assertEqual(sum(marker in (m.get('content') or '') for m in history), 1)
        self.assertTrue(any(marker in (m.get('content') or '')
                            for m in history if m['role'] == 'assistant'))
        self.assertFalse(any(m.get('tool_calls') for m in history))
        self.assertEqual(history[:len(generations[0]['messages'])], generations[0]['messages'])
        self.assertTrue(any(marker in (m.get('content') or '')
                            for m in self.archive() if m['role'] == 'assistant'))
        ready = next(b['state']['actions'][0] for _, b in self.calls
                     if 'action_0' in b.get('questions', {}).get('next', {}).get('criteria', {}))
        self.assertTrue(ready['verify'])
        self.assertIn('PREPARED_COMMAND_', ready['command_preview'])
        self.assertLessEqual(len(ready['command_preview']), 600)

    def test_decision_pruning_preserves_generator_prefix_and_evidence(self):
        generations = []
        plan = {'actions': [
            {'description': f'observe {i}',
             'command': "printf '" + f'RETAIN_FOR_GENERATOR_{i}_' + 'x' * 1700 + "'",
             'after': [i-1] if i else []} for i in range(5)], 'answer': None}
        def generate(body):
            generations.append(body)
            return plan if len(generations) == 1 else {'actions': [], 'answer': 'done'}
        self.generate = generate
        result = self.run_agent('Preserve this task exactly.')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(len(generations), 2)
        self.assertTrue(any('questions' in b and 'next' not in b['questions'] for _, b in self.calls))
        before, after = (b['messages'] for b in generations)
        self.assertEqual(after[:len(before)], before)
        self.assertEqual(json.loads(after[len(before)]['content']), plan)
        observations = [json.loads(m['content']) for m in after
                        if m['role'] == 'user' and m['content'].startswith('{')]
        for i in range(5):
            self.assertTrue(any(f'RETAIN_FOR_GENERATOR_{i}_' + 'x' * 1700 in o.get('result', '')
                                for o in observations))

    def test_long_generation_history_does_not_trigger_decision_pruning(self):
        self.config.write_text('max_turns=10\ndecision_context_bytes=6000\n')
        generations = []
        def generate(body):
            generations.append(body)
            return ({'actions': [{'description': 'read a large observation',
                     'command': "python3 -c 'print(\"x\" * 13000)'"}], 'answer': None}
                    if len(generations) == 1 else {'actions': [], 'answer': 'done'})
        self.generate = generate
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(len(generations), 2)
        self.assertGreater(len(json.dumps(generations[1]['messages'])), 13000)
        self.assertFalse(any('questions' in b and 'next' not in b['questions'] for _, b in self.calls))

    def test_generator_seal_uses_router_signal_and_preserves_later_observations(self):
        self.config.write_text('max_turns=10\ndecision_context_bytes=24000\n'
                              'compact_extra={"policy_ir":["generator-seal"],"keep_recent":4}\n')
        generations = []
        self.compact = lambda body: {'messages': body['messages'][:3] + [
            {'role': 'system', 'content': 'SEALED_GENERATION_HISTORY'}]}
        def generate(body):
            generations.append(body)
            self.signal_compact = len(generations) == 1
            return ({'actions': [{'description': 'observe after seal snapshot',
                                 'command': 'sleep 0.2; printf OBSERVED_AFTER_SNAPSHOT'}], 'answer': None}
                    if self.signal_compact else {'actions': [], 'answer': 'done'})
        self.generate = generate
        result = self.run_agent('Preserve original requirement.')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        seals = [b for p, b in self.calls if p == '/v1/compact']
        self.assertEqual(len(seals), 1)
        self.assertEqual(seals[0]['policy_ir'], ['generator-seal'])
        history = str(generations[1]['messages'])
        self.assertIn('SEALED_GENERATION_HISTORY', history)
        self.assertIn('OBSERVED_AFTER_SNAPSHOT', history)
        self.assertIn('Preserve original requirement.', history)
        last = [b for _, b in self.calls if 'next' in b.get('questions', {})][-1]
        self.assertNotIn('SEALED_GENERATION_HISTORY', str(last['state']))

    def test_generation_flow_is_forwarded_without_client_model_selection(self):
        flow = ['flow', {'fixture': 'router owns routing'}]
        result = self.run_agent(extra={'flow_ir': flow})
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        chat = next(b for p, b in self.calls if p == '/v1/chat/completions')
        self.assertEqual(chat['flow_ir'], flow)
        self.assertNotIn('model', chat)
        self.assertTrue(all('flow_ir' not in b for p, b in self.calls if p == '/v1/decisions'))

    def test_controller_selects_economy_or_capable_without_another_routing_decision(self):
        generations = []
        def generate(body):
            generations.append(body)
            if len(generations) == 1:
                return {'actions': [{'description': 'inspect', 'command': 'printf observed'}], 'answer': None}
            return {'actions': [], 'answer': 'done'}
        self.generate = generate
        def decide(body):
            options = body['questions']['next']['criteria']
            choice = ('finish' if 'finish' in options else 'action_0' if 'action_0' in options else
                      'generate_economy' if not generations else 'generate_capable')
            return decision_reply(body, choice)
        self.decide = decide
        result = self.run_agent(extra={'policy_ir': ['capable']}, economy={'policy_ir': ['economy']})
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual([b['policy_ir'] for b in generations], [['economy'], ['capable']])
        self.assertEqual([p for p, _ in self.calls], ['/v1/decisions', '/v1/chat/completions',
                         '/v1/decisions', '/v1/decisions', '/v1/chat/completions', '/v1/decisions'])
        self.assertTrue(all('flow_ir' not in b for _, b in self.calls))

    def test_unified_generation_rejects_nested_flow_before_inference(self):
        result = self.run_agent(extra={'flow_ir': ['flow', {}]}, economy={'policy_ir': ['economy']})
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.calls)

    def test_generated_discovery_reuses_procedure_with_literal_arguments(self):
        filenames = ['first.txt', "second'$(touch INJECTED).txt"]
        for i, name in enumerate(filenames): (self.home / name).write_text(f'evidence {i}')
        procedure = {'description': 'Read an unread discovered file', 'repeat': True,
            'command': 'cat -- "$1"', 'parameters': [{'description': 'Unread file path', 'values': filenames}]}
        discovery = "python3 -c " + shlex.quote('import json,pathlib; a='+repr(procedure)+
            '; a["parameters"][0]["values"]=sorted(p.name for p in pathlib.Path(".").glob("*.txt")); print(json.dumps([a]))')
        generations = []
        def generate(body):
            generations.append(body)
            return ({'actions': [{'description': 'Discover readable files', 'command': discovery, 'discover': True}], 'answer': None}
                    if len(generations) == 1 else {'actions': [], 'answer': 'read both'})
        self.generate = generate
        reads = 0
        def decide(body):
            nonlocal reads
            if 'action_0_arg_0' not in body['questions']: return decision_reply(body)
            if reads == 2: return decision_reply(body, 'generate')
            reply = decision_reply(body, 'action_0')
            arg = reply['answers']['action_0_arg_0']; pick = f'value_{reads}'
            arg['choice'] = pick; arg['probabilities'] = {k: float(k == pick) for k in arg['probabilities']}
            reads += 1
            return reply
        self.decide = decide
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(len(generations), 2)  # bootstrap once, final content once; no generation between reads
        self.assertEqual(reads, 2)
        self.assertFalse((self.home / 'INJECTED').exists())
        self.assertIn('evidence 0', str(generations[-1]['messages']))
        self.assertIn('evidence 1', str(generations[-1]['messages']))
        self.assertEqual(sum(m['role'] == 'tool' for m in self.archive()), 3)

    def test_invalid_selected_argument_cannot_execute_command(self):
        self.generate = lambda _: {'actions': [{'description': 'parameter fixture', 'command': 'touch forbidden',
            'parameters': [{'description': 'value', 'values': ['one', 'two']}]}], 'answer': None}
        def decide(body):
            reply = decision_reply(body)
            if 'action_0_arg_0' in reply['answers']:
                reply['answers']['action_0_arg_0']['choice'] = 'invented_value'
            return reply
        self.decide = decide
        result = self.run_agent()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.home / 'forbidden').exists())

    def test_named_procedure_survives_replan_and_discovery(self):
        (self.home / 'source.txt').write_text('first-evidence')
        generations = []
        reads = 0
        mutation = [{'description': 'Refresh source', 'command': 'printf second-evidence > source.txt'}]
        def generate(body):
            generations.append(body)
            if len(generations) == 1:
                return {'actions': [{'procedure': 'reader', 'description': 'Read source', 'repeat': True,
                        'command': 'cat -- "$1"', 'parameters': [{'description': 'Source file', 'values': ['source.txt']}]}]}
            if len(generations) == 2:
                return {'actions': [{'description': 'Discover mutation', 'discover': True,
                        'command': 'printf %s ' + shlex.quote(json.dumps(mutation))}]}
            return {'actions': [], 'answer': 'read fresh observations using the retained procedure'}
        def decide(body):
            nonlocal reads
            if 'next' not in body['questions']: return decision_reply(body)
            if 'finish' in body['questions']['next']['criteria']: return decision_reply(body, 'finish')
            for a in body['state']['actions']:
                if a['ready'] and a['description'] in ('Discover mutation', 'Refresh source'):
                    return decision_reply(body, a['id'])
            for a in body['state']['actions']:
                if a.get('procedure') == 'reader' and a['ready'] and reads < len(generations):
                    reads += 1
                    return decision_reply(body, a['id'])
            return decision_reply(body, 'generate')
        self.generate = generate; self.decide = decide
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(reads, 2)
        self.assertEqual(len(generations), 3)
        self.assertIn('first-evidence', str(generations[-1]['messages']))
        self.assertIn('second-evidence', str(generations[-1]['messages']))
        self.assertEqual(sum(m['role'] == 'tool' for m in self.archive()), 4)

    def test_forced_generation_skips_inference_and_decision_history_omits_programs(self):
        marker = 'PROGRAM_BODY_' + 'x' * 1200
        self.generate = lambda _: {'actions': [{'description': 'Produce evidence',
            'command': "printf %s '" + marker + "' > artifact; printf observed", 'verify': True}], 'answer': 'done'}
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(self.calls[0][0], '/v1/chat/completions')
        for _, body in self.calls:
            if 'next' in body.get('questions', {}):
                self.assertNotEqual(set(body['questions']['next']['criteria']), {'generate'})
                self.assertNotIn('PROGRAM_BODY_', str(body['state']['history']))
        self.assertIn(marker, str(self.archive()))

    def test_rejected_tool_response_is_archived_but_never_executed_or_replayed(self):
        generations = []
        tool = {'id': 'rejected-call', 'type': 'function',
                'function': {'name': 'shell', 'arguments': '{"command":"touch forbidden"}'}}
        def generate(body):
            generations.append(body)
            self.generated_tool_calls = [tool] if len(generations) == 1 else None
            return {'actions': [], 'answer': 'recovered'}
        self.generate = generate
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertFalse((self.home / 'forbidden').exists())
        self.assertEqual(len(generations), 2)
        self.assertFalse(any(m.get('tool_calls') for m in generations[1]['messages']))
        self.assertTrue(any(m.get('tool_calls') == [tool] for m in self.archive()))

    def test_truncated_plan_is_not_executed_and_recovery_gets_the_reason(self):
        calls = []
        def generate(body):
            calls.append(body)
            self.assertEqual(body['response_format'], {'type': 'json_object'})
            if len(calls) == 1:
                self.finish_reason = 'length'
                return {'actions': [{'description': 'must not execute partial plan', 'command': 'touch forbidden'}]}
            self.finish_reason = 'stop'
            self.assertIn('output-token limit', str(body['messages']))
            return {'actions': [], 'answer': 'recovered'}
        self.generate = generate
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(len(calls), 2)
        self.assertFalse((self.home / 'forbidden').exists())

    def test_multiple_json_objects_are_rejected_without_executing_first_plan(self):
        self.config.write_text('max_turns=2\n')
        self.generate = lambda _: json.dumps({'actions': [{'description': 'invalid response',
            'command': 'touch forbidden'}]}) + '\n' + json.dumps({'actions': [], 'answer': 'second draft'})
        result = self.run_agent()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.home / 'forbidden').exists())

    def test_provider_configuration_is_scrubbed_from_selected_shell(self):
        (self.skills / 'actions.json').write_text(json.dumps([{'description': 'verify scrubbed environment',
            'command': 'test -z "$SUBZEROCLAW_DECISION_EXTRA$SUBZEROCLAW_API_KEY$SUBZEROCLAW_REQUEST_EXTRA"', 'verify': True}]))
        result = self.run_agent()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertTrue(any(m['role'] == 'tool' and '[exit:0]' in m['content'] for m in self.archive()))


if __name__ == '__main__':
    unittest.main()
