import math

import pandas as pd
import pytest
from backtester_data.validation import (
    validate_canonical_data,
    validate_provider_data,
)


def _make_provider_data() -> pd.DataFrame:
    columns = pd.MultiIndex.from_product(
        [
            ["SPY"],
            [
                "Open",
                "High",
                "Low",
                "Close",
                "Volume",
                "Adj Close",
                "Dividends",
                "Stock Splits",
            ],
        ]
    )

    return pd.DataFrame(
        [
            [100.0, 105.0, 99.0, 103.0, 1_000_000, 102.5, 0.0, 0.0],
            [103.0, 106.0, 102.0, 105.0, 1_100_000, 104.5, 0.5, 0.0],
            [105.0, 107.0, 104.0, 106.0, 900_000, 105.5, 0.0, 0.0],
        ],
        index=pd.DatetimeIndex(
            ["2026-08-24", "2026-08-25", "2026-08-26"],
            name="Date",
        ),
        columns=columns,
    )


def _make_canonical_data() -> pd.DataFrame:
    return pd.DataFrame(
        {
            "session_date": ["2026-08-24", "2026-08-25", "2026-08-26"],
            "symbol": ["SPY", "SPY", "SPY"],
            "open": [100.0, 103.0, 105.0],
            "high": [105.0, 106.0, 107.0],
            "low": [99.0, 102.0, 104.0],
            "close": [103.0, 105.0, 106.0],
            "volume": [1_000_000, 1_100_000, 900_000],
        }
    )


# ---------------------------------------------------------------------------
# Provider validation
# ---------------------------------------------------------------------------


def test_validate_provider_data_accepts_valid_dataset() -> None:
    data = _make_provider_data()
    validate_provider_data(data, "SPY")


def test_validate_provider_data_rejects_empty_dataset() -> None:
    data = _make_provider_data().iloc[0:0]

    with pytest.raises(ValueError, match="Provider dataset cannot be empty"):
        validate_provider_data(data, "SPY")


@pytest.mark.parametrize("symbol", ["", "   "])
def test_validate_provider_data_rejects_empty_symbol(symbol: str) -> None:
    with pytest.raises(ValueError, match="Symbol cannot be empty"):
        validate_provider_data(_make_provider_data(), symbol)


@pytest.mark.parametrize("symbol", [" SPY", "SPY "])
def test_validate_provider_data_rejects_symbol_whitespace(symbol: str) -> None:
    with pytest.raises(
        ValueError,
        match="Symbol cannot contain leading or trailing whitespace",
    ):
        validate_provider_data(_make_provider_data(), symbol)


def test_validate_provider_data_requires_multiindex_columns() -> None:
    data = pd.DataFrame(
        {
            "Open": [100.0],
            "High": [105.0],
            "Low": [99.0],
            "Close": [103.0],
            "Volume": [1_000_000],
        },
        index=pd.DatetimeIndex(["2026-08-24"]),
    )

    with pytest.raises(
        ValueError,
        match="Provider dataset must use MultiIndex columns",
    ):
        validate_provider_data(data, "SPY")


def test_validate_provider_data_requires_requested_symbol() -> None:
    data = _make_provider_data()

    with pytest.raises(
        ValueError,
        match="Provider dataset does not contain symbol 'QQQ'",
    ):
        validate_provider_data(data, "QQQ")


def test_validate_provider_data_requires_all_ohlcv_columns() -> None:
    data = _make_provider_data().drop(columns=[("SPY", "Volume")])

    with pytest.raises(
        ValueError,
        match="Provider dataset is missing required columns: Volume",
    ):
        validate_provider_data(data, "SPY")


def test_validate_provider_data_requires_datetime_index() -> None:
    data = _make_provider_data()
    data.index = pd.Index(["2026-08-24", "2026-08-25", "2026-08-26"])

    with pytest.raises(
        ValueError,
        match="Provider dataset index must be a DatetimeIndex",
    ):
        validate_provider_data(data, "SPY")


def test_validate_provider_data_rejects_invalid_session_date() -> None:
    data = _make_provider_data()
    data.index = pd.DatetimeIndex(["2026-08-24", pd.NaT, "2026-08-26"])

    with pytest.raises(
        ValueError,
        match="Provider dataset contains invalid session dates",
    ):
        validate_provider_data(data, "SPY")


def test_validate_provider_data_rejects_duplicate_dates() -> None:
    data = _make_provider_data()
    data.index = pd.DatetimeIndex(["2026-08-24", "2026-08-24", "2026-08-26"])

    with pytest.raises(
        ValueError,
        match="Provider dataset contains duplicate session dates",
    ):
        validate_provider_data(data, "SPY")


def test_validate_provider_data_rejects_non_monotonic_dates() -> None:
    data = _make_provider_data()
    data.index = pd.DatetimeIndex(["2026-08-24", "2026-08-26", "2026-08-25"])

    with pytest.raises(
        ValueError,
        match="Provider session dates must be strictly increasing",
    ):
        validate_provider_data(data, "SPY")


def test_validate_provider_data_rejects_missing_required_value() -> None:
    data = _make_provider_data()
    data.loc[pd.Timestamp("2026-08-25"), ("SPY", "High")] = math.nan

    with pytest.raises(
        ValueError,
        match="Provider dataset contains missing required values",
    ):
        validate_provider_data(data, "SPY")


def test_validate_provider_data_allows_missing_optional_value() -> None:
    data = _make_provider_data()
    data.loc[pd.Timestamp("2026-08-25"), ("SPY", "Adj Close")] = math.nan
    validate_provider_data(data, "SPY")


# ---------------------------------------------------------------------------
# Canonical validation
# ---------------------------------------------------------------------------


def test_validate_canonical_data_accepts_valid_dataset() -> None:
    validate_canonical_data(_make_canonical_data())


@pytest.mark.parametrize(
    "columns",
    [
        ["session_date", "symbol", "open", "high", "low", "close"],
        ["session_date", "symbol", "open", "high", "low", "close", "volume", "extra"],
        ["symbol", "session_date", "open", "high", "low", "close", "volume"],
    ],
)
def test_validate_canonical_data_requires_exact_schema(columns: list[str]) -> None:
    data = _make_canonical_data()

    if "extra" in columns:
        data["extra"] = 1

    data = data[columns]

    with pytest.raises(
        ValueError,
        match="Canonical dataset must contain exactly these columns",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_rejects_empty_dataset() -> None:
    data = _make_canonical_data().iloc[0:0]

    with pytest.raises(ValueError, match="Canonical dataset cannot be empty"):
        validate_canonical_data(data)


def test_validate_canonical_data_rejects_missing_values() -> None:
    data = _make_canonical_data()
    data.loc[1, "close"] = math.nan

    with pytest.raises(ValueError, match="Canonical dataset contains missing values"):
        validate_canonical_data(data)


def test_validate_canonical_data_requires_string_dates() -> None:
    data = _make_canonical_data()
    data["session_date"] = data["session_date"].astype(object)
    data.loc[0, "session_date"] = 20260824

    with pytest.raises(ValueError, match="Canonical session dates must be strings"):
        validate_canonical_data(data)


def test_validate_canonical_data_requires_exact_date_format() -> None:
    data = _make_canonical_data()
    data.loc[0, "session_date"] = "2026-8-24"

    with pytest.raises(
        ValueError,
        match="Canonical session dates must use YYYY-MM-DD format",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_rejects_invalid_calendar_date() -> None:
    data = _make_canonical_data()
    data.loc[0, "session_date"] = "2026-02-30"

    with pytest.raises(
        ValueError,
        match="Canonical dataset contains invalid session dates",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_rejects_duplicate_dates() -> None:
    data = _make_canonical_data()
    data.loc[1, "session_date"] = "2026-08-24"

    with pytest.raises(
        ValueError,
        match="Canonical dataset contains duplicate session dates",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_rejects_non_monotonic_dates() -> None:
    data = _make_canonical_data()
    data["session_date"] = ["2026-08-24", "2026-08-26", "2026-08-25"]

    with pytest.raises(
        ValueError,
        match="Canonical session dates must be strictly increasing",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_requires_string_symbol() -> None:
    data = _make_canonical_data()
    data["symbol"] = data["symbol"].astype(object)
    data.loc[0, "symbol"] = 123

    with pytest.raises(ValueError, match="Canonical symbols must be strings"):
        validate_canonical_data(data)


@pytest.mark.parametrize("symbol", ["", "   "])
def test_validate_canonical_data_rejects_empty_symbol(symbol: str) -> None:
    data = _make_canonical_data()
    data.loc[0, "symbol"] = symbol

    with pytest.raises(ValueError, match="Canonical symbol cannot be empty"):
        validate_canonical_data(data)


@pytest.mark.parametrize("symbol", [" SPY", "SPY "])
def test_validate_canonical_data_rejects_symbol_whitespace(symbol: str) -> None:
    data = _make_canonical_data()
    data.loc[0, "symbol"] = symbol

    with pytest.raises(
        ValueError,
        match="Canonical symbol cannot contain leading or trailing whitespace",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_requires_single_symbol() -> None:
    data = _make_canonical_data()
    data.loc[2, "symbol"] = "QQQ"

    with pytest.raises(
        ValueError,
        match="Canonical dataset must contain exactly one symbol",
    ):
        validate_canonical_data(data)


@pytest.mark.parametrize("column", ["open", "high", "low", "close"])
def test_validate_canonical_data_requires_numeric_prices(column: str) -> None:
    data = _make_canonical_data()
    data[column] = data[column].astype(object)
    data.loc[0, column] = "invalid"

    with pytest.raises(
        ValueError,
        match=rf"Canonical column '{column}' must be numeric",
    ):
        validate_canonical_data(data)


@pytest.mark.parametrize("column", ["open", "high", "low", "close"])
def test_validate_canonical_data_requires_finite_prices(column: str) -> None:
    data = _make_canonical_data()
    data.loc[0, column] = math.inf

    with pytest.raises(
        ValueError,
        match=rf"Canonical column '{column}' must contain only finite values",
    ):
        validate_canonical_data(data)


@pytest.mark.parametrize("column", ["open", "high", "low", "close"])
@pytest.mark.parametrize("value", [0.0, -1.0])
def test_validate_canonical_data_requires_positive_prices(
    column: str,
    value: float,
) -> None:
    data = _make_canonical_data()
    data.loc[0, column] = value

    with pytest.raises(
        ValueError,
        match=rf"Canonical column '{column}' must contain positive values",
    ):
        validate_canonical_data(data)


@pytest.mark.parametrize(
    ("column", "value", "message"),
    [
        ("high", 99.0, "Canonical high cannot be lower than open"),
        ("high", 102.0, "Canonical high cannot be lower than close"),
        ("low", 101.0, "Canonical low cannot be higher than open"),
    ],
)
def test_validate_canonical_data_rejects_invalid_ohlc_relationships(
    column: str,
    value: float,
    message: str,
) -> None:
    data = _make_canonical_data()
    data.loc[0, column] = value

    with pytest.raises(ValueError, match=message):
        validate_canonical_data(data)


def test_validate_canonical_data_rejects_low_above_close() -> None:
    data = _make_canonical_data()
    data.loc[0, "open"] = 105.0
    data.loc[0, "high"] = 105.0
    data.loc[0, "low"] = 104.0
    data.loc[0, "close"] = 103.0

    with pytest.raises(
        ValueError,
        match="Canonical low cannot be higher than close",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_rejects_high_below_low() -> None:
    data = _make_canonical_data()
    data.loc[0, "open"] = 100.0
    data.loc[0, "close"] = 100.0
    data.loc[0, "low"] = 99.0
    data.loc[0, "high"] = 98.0

    with pytest.raises(
        ValueError,
        match="Canonical high cannot be lower than",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_requires_numeric_volume() -> None:
    data = _make_canonical_data()
    data["volume"] = data["volume"].astype(object)
    data.loc[0, "volume"] = "invalid"

    with pytest.raises(ValueError, match="Canonical volume must be numeric"):
        validate_canonical_data(data)


def test_validate_canonical_data_requires_finite_volume() -> None:
    data = _make_canonical_data()
    data["volume"] = data["volume"].astype(float)
    data.loc[0, "volume"] = math.inf

    with pytest.raises(
        ValueError,
        match="Canonical volume must contain only finite values",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_rejects_negative_volume() -> None:
    data = _make_canonical_data()
    data.loc[0, "volume"] = -1

    with pytest.raises(ValueError, match="Canonical volume cannot be negative"):
        validate_canonical_data(data)


def test_validate_canonical_data_rejects_fractional_volume() -> None:
    data = _make_canonical_data()
    data["volume"] = data["volume"].astype(float)
    data.loc[0, "volume"] = 1000.5

    with pytest.raises(
        ValueError,
        match="Canonical volume must contain integer values",
    ):
        validate_canonical_data(data)


def test_validate_canonical_data_accepts_zero_volume() -> None:
    data = _make_canonical_data()
    data.loc[0, "volume"] = 0
    validate_canonical_data(data)
