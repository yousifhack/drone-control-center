"""Protocol/provider tests; HTTP responses here are fixtures, not model-quality evidence."""

import importlib.util
import json
import unittest
from pathlib import Path
from unittest.mock import MagicMock

spec = importlib.util.spec_from_file_location(
    "worker", Path(__file__).parents[1] / "ai" / "worker.py"
)
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)


class WorkerTest(unittest.TestCase):
    def setUp(self):
        self.context = {"capabilities": [{"capabilityId": "cargo/gripper.open", "available": True}]}

    def test_duplicate_keys_and_nonfinite(self):
        for text in ['{"a":1,"a":2}', '{"a":NaN}', '{"a":Infinity}']:
            with self.assertRaises(ValueError):
                worker.decode(text)

    def test_local_endpoint_only(self):
        for url in [
            "https://api.example.com",
            "http://localhost:8080",
            "http://127.0.0.1:8080/other",
            "http://127.0.0.1:8080?api_key=secret",
            "http://a:b@127.0.0.1:8080",
        ]:
            with self.assertRaises(ValueError):
                worker.LlamaProvider(url)
        worker.LlamaProvider("http://127.0.0.1:8080")

    def test_unknown_capability(self):
        with self.assertRaises(ValueError):
            worker.validate_result(
                {"kind": "actionProposal", "capabilityId": "motor/raw", "arguments": {}},
                self.context,
            )

    def test_forbidden_extra_fields(self):
        with self.assertRaises(ValueError):
            worker.validate_result(
                {
                    "kind": "actionProposal",
                    "capabilityId": "cargo/gripper.open",
                    "arguments": {},
                    "approved": True,
                },
                self.context,
            )

    def test_valid_proposal_is_not_execution(self):
        result = {"kind": "actionProposal", "capabilityId": "cargo/gripper.open", "arguments": {}}
        self.assertEqual(worker.validate_result(result, self.context), result)

    def test_truncated_model_output_rejected(self):
        p = worker.LlamaProvider("http://127.0.0.1:8080")
        p.opener = MagicMock()
        p.opener.open.return_value.__enter__.return_value.read.return_value = json.dumps(
            {"choices": [{"finish_reason": "length", "message": {"content": "{}"}}]}
        ).encode()
        with self.assertRaises(ValueError):
            p.infer("open cargo", self.context)

    def test_http_request_and_valid_answer(self):
        p = worker.LlamaProvider("http://127.0.0.1:8080")
        p.opener = MagicMock()
        answer = {"kind": "answer", "text": "The context exposes a simulated cargo gripper."}
        p.opener.open.return_value.__enter__.return_value.read.return_value = json.dumps(
            {"choices": [{"finish_reason": "stop", "message": {"content": json.dumps(answer)}}]}
        ).encode()
        self.assertEqual(p.infer("what is connected", self.context), answer)
        request = p.opener.open.call_args.args[0]
        self.assertEqual(request.full_url, "http://127.0.0.1:8080/v1/chat/completions")
        self.assertIn("schema", json.loads(request.data)["response_format"])


if __name__ == "__main__":
    unittest.main()
