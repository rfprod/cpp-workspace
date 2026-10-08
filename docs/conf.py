from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

project = "cpp-workspace"
extensions = ["breathe"]

breathe_projects = {
    "cpp-workspace": str(ROOT / "build" / "doxygen" / "xml"),
}
breathe_default_project = "cpp-workspace"

html_theme = "sphinx_rtd_theme"
