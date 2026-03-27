import os
import shutil
import subprocess
import unittest
from pathlib import Path


THIS_DIR = Path(__file__).resolve().parent


def _has_host_c_compiler() -> bool:
    return shutil.which("gcc") is not None or shutil.which("clang") is not None


class RunTestScriptCase(unittest.TestCase):
    def test_lcd_mcu_st7735s_host_unit(self):
        if not _has_host_c_compiler():
            self.skipTest("gcc/clang not found in PATH")

        if os.name == "nt":
            shell = shutil.which("powershell") or shutil.which("pwsh")
            if shell is None:
                self.skipTest("PowerShell not found")
            cmd = [
                shell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(THIS_DIR / "run_test.ps1"),
            ]
        else:
            cmd = ["bash", str(THIS_DIR / "run_test.sh")]

        proc = subprocess.run(cmd, cwd=THIS_DIR, capture_output=True, text=True, check=False)
        self.assertEqual(
            0,
            proc.returncode,
            "run_test script failed\n"
            f"stdout:\n{proc.stdout}\n"
            f"stderr:\n{proc.stderr}\n",
        )
        self.assertIn("Done:", proc.stdout)


if __name__ == "__main__":
    unittest.main()
