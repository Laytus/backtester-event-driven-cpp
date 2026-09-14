import json
import math
from collections.abc import Mapping
from pathlib import Path

import pandas as pd
from pandas.api.types import is_numeric_dtype


def _prepare_parent_directory(path: Path) -> None:
    path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

def _prepare_canonical_for_serialization(
    data: pd.DataFrame,
) -> pd.DataFrame:
    if "volume" not in data.columns:
        raise ValueError(
            "Canonical dataset must contain a volume column"
        )

    if not is_numeric_dtype(data["volume"]):
        raise ValueError(
            "Canonical volume must be numeric before serialization"
        )

    if data["volume"].isna().any():
        raise ValueError(
            "Canonical volume cannot contain missing values before serialization"
        )

    if not data["volume"].map(math.isfinite).all():
        raise ValueError(
            "Canonical volume must contain only finite values before serialization"
        )

    if (data["volume"] < 0).any():
        raise ValueError(
            "Canonical volume cannot be negative before serialization"
        )

    if not data["volume"].map(
        lambda value: value % 1 == 0
    ).all():
        raise ValueError(
            "Canonical volume must contain integer values before serialization"
        )

    serialized = data.copy(deep=True)

    serialized["volume"] = serialized["volume"].astype("uint64")

    return serialized

def write_raw_data(
    data: pd.DataFrame,
    path: Path,
) -> None:
    _prepare_parent_directory(path)

    with path.open(
        "x",
        encoding="utf-8",
        newline="",
    ) as file:
        data.to_csv(
            file,
            index=True,
            lineterminator="\n",
        )

def write_metadata(
    metadata: Mapping[str, object],
    path: Path,
) -> None:
    _prepare_parent_directory(path)

    with path.open(
        "x",
        encoding="utf-8",
    ) as file:
        json.dump(
            metadata,
            file,
            indent=2,
            sort_keys=True,
            ensure_ascii=False,
        )

        file.write("\n")

def write_canonical_data(
    data: pd.DataFrame,
    path: Path,
) -> None:
    serialized = _prepare_canonical_for_serialization(data)

    _prepare_parent_directory(path)

    with path.open(
        "x",
        encoding="utf-8",
        newline="",
    ) as file:
        serialized.to_csv(
            file,
            index=False,
            lineterminator="\n"
        )