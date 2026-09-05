"""Exercise the shipped loop/protocol against an in-process HTTP provider."""
import json
import os
from pathlib import Path
import queue
import subprocess
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


BIN = Path(__file__).resolve().parents[1] / "subzeroclaw"


def answer(text="ok", compact=False, tool=False, finish="stop"):
    message = {"role": "assistant", "content": text}
    if tool:
        message["tool_calls"] = [{
            "id": "call-1", "type": "function",
            "function": {"name": "shell", "arguments": json.dumps({"command": "printf tool-ok"})},
        }]
    return {"choices": [{"finish_reason": finish, "message": message}],
            "x_router": {"compact": compact}}


class RuntimeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="szc-integration-")
        self.addCleanup(self.tmp.cleanup)
        self.home = Path(self.tmp.name)
        config = self.home / ".subzeroclaw"
        config.mkdir()
        (config / "config").write_text("max_turns=30\n")
        self.requests = []
        self.compactions = []
        self.compact_started = threading.Event()
        self.release_compact = threading.Event()
        self.addCleanup(self.release_compact.set)
        self.compact_status = 200
        self.compact_body = {"messages": [{"role": "system", "content": "SEALED"}]}
        self.chat = lambda body: answer()
        case = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                if self.path == "/v1/compact":
                    case.compactions.append(body)
                    response, status = case.compact_body, case.compact_status
                    case.compact_started.set()
                    case.release_compact.wait(10)
                else:
                    case.requests.append(body)
                    response, status = case.chat(body), 200
                payload = json.dumps(response).encode()
                try:
                    self.send_response(status)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(payload)))
                    self.end_headers()
                    self.wfile.write(payload)
                except (BrokenPipeError, ConnectionResetError):
                    pass

        self.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)
        # Allowlist: never inherit provider keys, proxies, or the user's config.
        env = {"PATH": os.environ["PATH"], "HOME": str(self.home),
               "SUBZEROCLAW_API_KEY": "local-fixture-only",
               "SUBZEROCLAW_ENDPOINT": f"http://127.0.0.1:{self.server.server_port}/v1/chat/completions",
               "SUBZEROCLAW_COMPACT_EXTRA": '{"keep_recent":1}'}
        self.proc = subprocess.Popen([str(BIN)], env=env, cwd=self.home,
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE)
        self.addCleanup(self.stop)
        self.lines = queue.Queue()

        def read_output():
            for line in iter(self.proc.stdout.readline, b""):
                self.lines.put(line)
            self.lines.put(b"")

        threading.Thread(target=read_output, daemon=True).start()

    def stop(self):
        if self.proc.poll() is None:
            self.proc.stdin.close()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=3)
                self.fail("runtime failed to shut down")
        for stream in (self.proc.stdin, self.proc.stdout, self.proc.stderr):
            stream.close()

    def turn(self, prompt):
        self.proc.stdin.write(prompt.encode() + b"\0")
        self.proc.stdin.flush()
        output = b""
        while True:
            line = self.lines.get(timeout=5)
            self.assertTrue(line, "runtime exited before turn completion")
            output += line
            if b"<<TURN_COMPLETE>>" in line:
                return output

    def log(self):
        return "".join(p.read_text() for p in (self.home / ".subzeroclaw/logs").glob("*.txt"))

    def wait_for_seal(self, compact_again=False):
        """Additional requests bound polling without assuming a scheduler delay."""
        sealed = any(m.get("content") == "SEALED" for m in self.requests[-1]["messages"])
        if sealed:
            return answer("sealed-ok")
        # One cheap local command gives the completed background process CPU time.
        result = answer(tool=True, compact=compact_again)
        result["choices"][0]["message"]["tool_calls"][0]["function"]["arguments"] = '{"command":"sleep 0.02"}'
        return result

    def test_compaction_survives_turns_and_keeps_new_input(self):
        self.chat = lambda body: answer(compact=True)
        self.turn("first")
        self.assertTrue(self.compact_started.wait(3))
        self.assertEqual(len(self.compactions), 1)
        self.release_compact.set()
        self.chat = lambda body: self.wait_for_seal()
        self.assertIn(b"sealed-ok", self.turn("second"))
        self.assertIn({"role": "user", "content": "second"}, self.requests[-1]["messages"])
        self.assertIn("context compacted", self.log())

    def test_failed_compaction_retries_without_losing_history(self):
        self.compact_status = 503
        self.release_compact.set()
        self.chat = lambda body: answer(compact=True)
        self.turn("first")
        self.assertTrue(self.compact_started.wait(3))
        self.compact_status = 200
        self.chat = lambda body: self.wait_for_seal(compact_again=True)
        self.assertIn(b"sealed-ok", self.turn("second"))
        self.assertGreaterEqual(len(self.compactions), 2)
        self.assertIn("compaction failed; history retained", self.log())

    def test_snapshot_has_paired_tools_and_finish_alias_runs_tool(self):
        def chat(body):
            if len(self.requests) == 1:
                return answer(tool=True, compact=True, finish="tool_use")
            return answer("tool-finished")
        self.chat = chat
        self.assertIn(b"tool-finished", self.turn("first"))
        self.assertTrue(self.compact_started.wait(3))
        messages = self.compactions[0]["messages"]
        self.assertEqual(messages[-1]["role"], "tool")
        self.assertEqual(messages[-1]["tool_call_id"], "call-1")
        self.assertIn("tool-ok", messages[-1]["content"])

    def test_one_pending_compaction_and_shutdown_cleanup(self):
        self.chat = lambda body: answer(compact=True)
        self.turn("first")
        self.assertTrue(self.compact_started.wait(3))
        self.turn("second")
        self.assertEqual(len(self.compactions), 1)
        # Linux procfs identifies only this runtime's own private directory.
        children_path = Path(f"/proc/{self.proc.pid}/task/{self.proc.pid}/children")
        if not children_path.exists():
            self.skipTest("process cleanup inspection requires Linux procfs")
        children = children_path.read_text().split()
        self.assertEqual(len(children), 1)
        child = children[0]
        curl_children = Path(f"/proc/{child}/task/{child}/children").read_text().split()
        self.assertEqual(len(curl_children), 1)
        args = Path(f"/proc/{curl_children[0]}/cmdline").read_bytes().split(b"\0")
        body_arg = args[args.index(b"--data-binary") + 1].decode()
        paths = [Path(body_arg[1:]).parent]
        self.assertTrue(paths)
        self.stop()
        self.assertFalse(Path(f"/proc/{child}").exists())
        self.assertTrue(all(not path.exists() for path in paths))

    def test_empty_compaction_does_not_erase_history(self):
        self.compact_body = {"messages": []}
        self.release_compact.set()
        self.chat = lambda body: answer(compact=True)
        self.turn("first")
        self.assertTrue(self.compact_started.wait(3))
        def chat(body):
            if "invalid compaction response" in self.log():
                return answer("retained-ok")
            return self.wait_for_seal()
        self.chat = chat
        self.assertIn(b"retained-ok", self.turn("second"))
        self.assertIn({"role": "user", "content": "first"}, self.requests[-1]["messages"])

    def test_endpoint_metacharacters_cannot_execute_a_host_command(self):
        marker = self.home / "must-not-exist"
        endpoint = (f"http://127.0.0.1:{self.server.server_port}/v1/chat/completions"
                    f"'; touch '{marker}'; #")
        env = {"PATH": os.environ["PATH"], "HOME": str(self.home),
               "SUBZEROCLAW_API_KEY": "local-fixture-only",
               "SUBZEROCLAW_ENDPOINT": endpoint}
        result = subprocess.run([str(BIN), "test"], env=env, cwd=self.home,
                                capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(marker.exists())


if __name__ == "__main__":
    unittest.main()
