from pathlib import Path


def test_data_directories_exist() -> None:
    project_root = Path(__file__).parents[2]

    assert (project_root / "data" / "raw").is_dir()
    assert (project_root / "data" / "processed").is_dir()

