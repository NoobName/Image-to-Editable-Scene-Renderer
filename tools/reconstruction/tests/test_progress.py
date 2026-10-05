import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch
from pipeline.progress import ProgressReporter, STAGES
from pipeline.runner import ReconstructionPipeline
from .support import make_input


class ProgressTests(unittest.TestCase):
    def test_transient_windows_replace_contention_retries(self):
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / "progress.json"
            import os
            replace = os.replace
            with patch("pipeline.progress.os.replace", side_effect=[PermissionError("sharing"), None]) as mocked:
                with patch("pipeline.progress.time.sleep"):
                    ProgressReporter(path, "retry")
                self.assertEqual(mocked.call_count, 2)
            replace(path.with_name("progress.json.tmp"), path)
            self.assertEqual(json.loads(path.read_text(encoding="utf-8"))["job_id"], "retry")

    def test_atomic_snapshot_identity_and_completion(self):
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / "job/progress.json"
            reporter = ProgressReporter(path, "id-1")
            with self.assertRaises(ValueError):
                reporter.finish()
            for name in STAGES:
                reporter.stage(name, "running")
                reporter.stage(name, "complete")
            reporter.finish()
            snapshot = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(snapshot["sequence"], 2+2*len(STAGES))
            self.assertEqual(snapshot["job_id"], "id-1")
            self.assertEqual(snapshot["state"], "complete")
            self.assertFalse(path.with_name("progress.json.tmp").exists())

    def test_pipeline_order_and_export_published_before_complete(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = make_input(root / "image.png", (32, 32))
            events = []
            def event(name, state, detail):
                events.append((name, state))
                if name == "export" and state == "complete":
                    self.assertTrue((root / "output/scene.json").is_file())
            ReconstructionPipeline().run(source, root / "output", stage_event=event, progress=lambda text: None)
            self.assertEqual(events, [(stage, state) for stage in STAGES for state in ("running", "complete")])

    def test_failure_does_not_mark_later_stages_complete(self):
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / "progress.json"
            report = ProgressReporter(path)
            report.stage("geometry", "running")
            report.fail("模型不可用：中文路径")
            data = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(data["stages"]["geometry"], "error")
            self.assertEqual(data["stages"]["materials"], "pending")
            self.assertEqual(data["state"], "error")
            self.assertEqual(data["detail"], "模型不可用：中文路径")

    def test_cli_missing_image_reports_error_and_no_package(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            run = subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / "reconstruct.py"),
                str(root / "missing.png"), "--output", str(root / "output"), "--progress-file", str(root / "job/progress.json"),
                "--job-id", "failure-case"], capture_output=True)
            self.assertNotEqual(run.returncode, 0)
            report = json.loads((root / "job/progress.json").read_text(encoding="utf-8"))
            self.assertEqual(report["state"], "error")
            self.assertEqual(report["job_id"], "failure-case")
            self.assertFalse((root / "output/scene.json").exists())
