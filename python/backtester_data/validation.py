import math

import pandas as pd
from pandas.api.types import is_numeric_dtype

_REQUIRED_PROVIDER_COLUMNS = {
    "Open",
    "High",
    "Low",
    "Close",
    "Volume",
}

_CANONICAL_COLUMNS = [
    "session_date",
    "symbol",
    "open",
    "high",
    "low",
    "close",
    "volume",
]

_PRICE_COLUMNS = [
    "open",
    "high",
    "low",
    "close",
]

def _is_empty_or_whitespace(value: object) -> bool:
    if not isinstance(value, str):
        return False

    return not value or value.isspace()

def _has_surrounding_whitespace(value: object) -> bool:
    if not isinstance(value, str):
        return False

    return value != value.strip()

def validate_provider_data(
    data: pd.DataFrame,
    symbol: str,
) -> None:
    if data.empty:
        raise ValueError(
            "Provider dataset cannot be empty"
        )

    if not symbol or symbol.isspace():
        raise ValueError(
            "Symbol cannot be empty"
        )

    if symbol != symbol.strip():
        raise ValueError(
            "Symbol cannot contain leading or trailing whitespace"
        )

    if not isinstance(data.columns, pd.MultiIndex):
        raise ValueError(
            "Provider dataset must use MultiIndex columns"
        )

    available_symbols = data.columns.get_level_values(0)

    if symbol not in available_symbols:
        raise ValueError(
            f"Provider dataset does not contain symbol '{symbol}'"
        )

    symbol_data = data[symbol]

    missing_columns = (
        _REQUIRED_PROVIDER_COLUMNS
        - set(symbol_data.columns)
    )

    if missing_columns:
        raise ValueError(
            "Provider dataset is missing required columns: "
            + ", ".join(sorted(missing_columns))
        )

    if not isinstance(data.index, pd.DatetimeIndex):
        raise ValueError(
            "Provider dataset index must be a DatetimeIndex"
        )

    if data.index.hasnans:
        raise ValueError(
            "Provider dataset contains invalid session dates"
        )

    if data.index.has_duplicates:
        raise ValueError(
            "Provider dataset contains duplicate session dates"
        )

    if not data.index.is_monotonic_increasing:
        raise ValueError(
            "Provider session dates must be strictly increasing"
        )

    required_data = symbol_data[
        ["Open", "High", "Low", "Close", "Volume"]
    ]

    if required_data.isna().any().any():
        raise ValueError(
            "Provider dataset contains missing required values"
        )

def validate_canonical_data(
    data: pd.DataFrame,
) -> None:
    if list(data.columns) != _CANONICAL_COLUMNS:
        raise ValueError(
            "Canonical dataset must contain exactly these columns "
            "in this order: "
            + ", ".join(_CANONICAL_COLUMNS)
        )

    if data.empty:
        raise ValueError(
            "Canonical dataset cannot be empty"
        )

    if data.isna().any().any():
        raise ValueError(
            "Canonical dataset contains missing values"
        )

    if not data["session_date"].map(
        lambda value: isinstance(value, str)
    ).all():
        raise ValueError(
            "Canonical session dates must be strings"
        )

    date_pattern = r"\d{4}-\d{2}-\d{2}"

    if not data["session_date"].str.fullmatch(date_pattern).all():
        raise ValueError(
            "Canonical session dates must use YYYY-MM-DD format"
        )

    try:
        parsed_dates = pd.to_datetime(
            data["session_date"],
            format="%Y-%m-%d",
            errors="raise",
        )
    except (ValueError, TypeError) as error:
        raise ValueError(
            "Canonical dataset contains invalid session dates"
        ) from error

    if parsed_dates.duplicated().any():
        raise ValueError(
            "Canonical dataset contains duplicate session dates"
        )

    if not parsed_dates.is_monotonic_increasing:
        raise ValueError(
            "Canonical session dates must be strictly increasing"
        )

    if not data["symbol"].map(
        lambda value: isinstance(value, str)
    ).all():
        raise ValueError(
            "Canonical symbols must be strings"
        )

    if data["symbol"].map(
        _is_empty_or_whitespace
    ).any():
        raise ValueError(
            "Canonical symbol cannot be empty"
        )

    if data["symbol"].map(
        _has_surrounding_whitespace
    ).any():
        raise ValueError(
            "Canonical symbol cannot contain leading or trailing whitespace"
        )

    if data["symbol"].nunique() != 1:
        raise ValueError(
            "Canonical dataset must contain exactly one symbol"
        )

    for column in _PRICE_COLUMNS:
        if not is_numeric_dtype(data[column]):
            raise ValueError(
                f"Canonical column '{column}' must be numeric"
            )

    for column in _PRICE_COLUMNS:
        if not data[column].map(math.isfinite).all():
            raise ValueError(
                f"Canonical column '{column}' must contain only finite values"
            )

    for column in _PRICE_COLUMNS:
        if (data[column] <= 0).any():
            raise ValueError(
                f"Canonical column '{column}' must contain positive values"
            )

    if (data["high"] < data["open"]).any():
        raise ValueError(
            "Canonical high cannot be lower than open"
        )

    if (data["high"] < data["close"]).any():
        raise ValueError(
            "Canonical high cannot be lower than close"
        )

    if (data["high"] < data["low"]).any():
        raise ValueError(
            "Canonical high cannot be lower than low"
        )

    if (data["low"] > data["open"]).any():
        raise ValueError(
            "Canonical low cannot be higher than open"
        )

    if (data["low"] > data["close"]).any():
        raise ValueError(
            "Canonical low cannot be higher than close"
        )

    if not is_numeric_dtype(data["volume"]):
        raise ValueError(
            "Canonical volume must be numeric"
        )

    if not data["volume"].map(math.isfinite).all():
        raise ValueError(
            "Canonical volume must contain only finite values"
        )

    if (data["volume"] < 0).any():
        raise ValueError(
            "Canonical volume cannot be negative"
        )

    if not data["volume"].map(
        lambda value: value % 1 == 0
    ).all():
        raise ValueError(
            "Canonical volume must contain integer values"
        )