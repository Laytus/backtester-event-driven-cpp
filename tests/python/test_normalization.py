import pandas as pd
import pytest
from backtester_data.normalization import (
    normalize_daily_bars,
    select_symbol,
)


def _make_raw_data() -> pd.DataFrame:
    columns = pd.MultiIndex.from_product(
        [
            ["SPY", "QQQ"],
            [
                "Open",
                "High",
                "Low",
                "Close",
                "Adj Close",
                "Volume",
                "Dividends",
                "Stock Splits",
            ],
        ]
    )

    return pd.DataFrame(
        [
            [
                # SPY
                100.0,
                105.0,
                99.0,
                103.0,
                80.0,
                1_000_000,
                0.5,
                2.0,
                # QQQ
                200.0,
                205.0,
                198.0,
                203.0,
                150.0,
                2_000_000,
                0.0,
                0.0,
            ],
            [
                # SPY
                103.0,
                106.0,
                102.0,
                105.0,
                81.0,
                1_100_000,
                0.0,
                0.0,
                # QQQ
                203.0,
                208.0,
                202.0,
                207.0,
                151.0,
                2_100_000,
                0.0,
                0.0,
            ],
            [
                # SPY
                105.0,
                107.0,
                104.0,
                106.0,
                82.0,
                900_000,
                0.0,
                0.0,
                # QQQ
                207.0,
                210.0,
                205.0,
                209.0,
                152.0,
                1_900_000,
                0.0,
                0.0,
            ],
        ],
        index=pd.DatetimeIndex(
            [
                "2026-08-24",
                "2026-08-25",
                "2026-08-26",
            ],
            name="Date",
        ),
        columns=columns,
    )


def _make_provider_data() -> pd.DataFrame:
    return _make_raw_data()["SPY"].copy()


# ---------------------------------------------------------------------------
# select_symbol
# ---------------------------------------------------------------------------


def test_select_symbol_returns_requested_symbol() -> None:
    raw_data = _make_raw_data()

    result = select_symbol(raw_data, "SPY")

    assert list(result.columns) == [
        "Open",
        "High",
        "Low",
        "Close",
        "Adj Close",
        "Volume",
        "Dividends",
        "Stock Splits",
    ]

    assert result.loc[pd.Timestamp("2026-08-24"), "Open"] == 100.0
    assert result.loc[pd.Timestamp("2026-08-24"), "Close"] == 103.0


def test_select_symbol_returns_independent_copy() -> None:
    raw_data = _make_raw_data()

    result = select_symbol(raw_data, "SPY")

    result.loc[pd.Timestamp("2026-08-24"), "Open"] = 999.0

    assert (
        raw_data.loc[
            pd.Timestamp("2026-08-24"),
            ("SPY", "Open"),
        ]
        == 100.0
    )


def test_select_symbol_rejects_non_multiindex_columns() -> None:
    raw_data = pd.DataFrame(
        {
            "Open": [100.0],
            "High": [105.0],
        }
    )

    with pytest.raises(
        ValueError,
        match="Provider dataset must use MultiIndex columns",
    ):
        select_symbol(raw_data, "SPY")


def test_select_symbol_rejects_missing_symbol() -> None:
    raw_data = _make_raw_data()

    with pytest.raises(
        ValueError,
        match="Provider dataset does not contain symbol 'AAPL'",
    ):
        select_symbol(raw_data, "AAPL")


# ---------------------------------------------------------------------------
# normalize_daily_bars
# ---------------------------------------------------------------------------


def test_normalize_daily_bars_produces_exact_schema() -> None:
    provider_data = _make_provider_data()

    result = normalize_daily_bars(
        provider_data,
        "SPY",
    )

    assert list(result.columns) == [
        "session_date",
        "symbol",
        "open",
        "high",
        "low",
        "close",
        "volume",
    ]


def test_normalize_daily_bars_preserves_dates_and_order() -> None:
    provider_data = _make_provider_data()

    result = normalize_daily_bars(
        provider_data,
        "SPY",
    )

    assert result["session_date"].tolist() == [
        "2026-08-24",
        "2026-08-25",
        "2026-08-26",
    ]


def test_normalize_daily_bars_assigns_requested_symbol() -> None:
    provider_data = _make_provider_data()

    result = normalize_daily_bars(
        provider_data,
        "SPY",
    )

    assert result["symbol"].tolist() == [
        "SPY",
        "SPY",
        "SPY",
    ]


def test_normalize_daily_bars_preserves_provider_ohlc() -> None:
    provider_data = _make_provider_data()

    result = normalize_daily_bars(
        provider_data,
        "SPY",
    )

    assert result["open"].tolist() == [
        100.0,
        103.0,
        105.0,
    ]

    assert result["high"].tolist() == [
        105.0,
        106.0,
        107.0,
    ]

    assert result["low"].tolist() == [
        99.0,
        102.0,
        104.0,
    ]

    assert result["close"].tolist() == [
        103.0,
        105.0,
        106.0,
    ]


def test_normalize_daily_bars_ignores_adjusted_close() -> None:
    provider_data = _make_provider_data()

    result = normalize_daily_bars(
        provider_data,
        "SPY",
    )

    assert provider_data["Adj Close"].tolist() == [
        80.0,
        81.0,
        82.0,
    ]

    assert result["close"].tolist() == [
        103.0,
        105.0,
        106.0,
    ]

    assert "Adj Close" not in result.columns


def test_normalize_daily_bars_ignores_corporate_action_columns() -> None:
    provider_data = _make_provider_data()

    result = normalize_daily_bars(
        provider_data,
        "SPY",
    )

    assert provider_data.loc[
        pd.Timestamp("2026-08-24"),
        "Dividends",
    ] == 0.5

    assert provider_data.loc[
        pd.Timestamp("2026-08-24"),
        "Stock Splits",
    ] == 2.0

    assert "Dividends" not in result.columns
    assert "Stock Splits" not in result.columns

    assert result.loc[0, "open"] == 100.0
    assert result.loc[0, "close"] == 103.0


def test_normalize_daily_bars_preserves_volume() -> None:
    provider_data = _make_provider_data()

    result = normalize_daily_bars(
        provider_data,
        "SPY",
    )

    assert result["volume"].tolist() == [
        1_000_000,
        1_100_000,
        900_000,
    ]


def test_normalize_daily_bars_does_not_mutate_provider_data() -> None:
    provider_data = _make_provider_data()
    expected = provider_data.copy(deep=True)

    normalize_daily_bars(
        provider_data,
        "SPY",
    )

    pd.testing.assert_frame_equal(
        provider_data,
        expected,
    )


def test_normalize_daily_bars_rejects_missing_required_column() -> None:
    provider_data = _make_provider_data().drop(
        columns=["Volume"]
    )

    with pytest.raises(
        ValueError,
        match=(
            "Cannot normalize provider data; "
            "missing required columns: Volume"
        ),
    ):
        normalize_daily_bars(
            provider_data,
            "SPY",
        )


def test_normalize_daily_bars_requires_datetime_index() -> None:
    provider_data = _make_provider_data()

    provider_data.index = pd.Index(
        [
            "2026-08-24",
            "2026-08-25",
            "2026-08-26",
        ]
    )

    with pytest.raises(
        ValueError,
        match="Provider dataset index must be a DatetimeIndex",
    ):
        normalize_daily_bars(
            provider_data,
            "SPY",
        )


@pytest.mark.parametrize(
    "symbol",
    [
        "",
        "   ",
    ],
)
def test_normalize_daily_bars_rejects_empty_symbol(
    symbol: str,
) -> None:
    provider_data = _make_provider_data()

    with pytest.raises(
        ValueError,
        match="Symbol cannot be empty",
    ):
        normalize_daily_bars(
            provider_data,
            symbol,
        )


@pytest.mark.parametrize(
    "symbol",
    [
        " SPY",
        "SPY ",
    ],
)
def test_normalize_daily_bars_rejects_symbol_whitespace(
    symbol: str,
) -> None:
    provider_data = _make_provider_data()

    with pytest.raises(
        ValueError,
        match="Symbol cannot contain leading or trailing whitespace",
    ):
        normalize_daily_bars(
            provider_data,
            symbol,
        )


def test_normalize_daily_bars_preserves_fractional_volume() -> None:
    provider_data = _make_provider_data()
    provider_data["Volume"] = provider_data["Volume"].astype(float)
    provider_data.loc[
        pd.Timestamp("2026-08-24"),
        "Volume",
    ] = 1000.5

    result = normalize_daily_bars(
        provider_data,
        "SPY",
    )

    assert result.loc[0, "volume"] == 1000.5