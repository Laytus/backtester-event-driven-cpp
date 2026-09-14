import pandas as pd

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

def select_symbol(
    raw_data: pd.DataFrame,
    symbol: str,
) -> pd.DataFrame:
    # Validate MultiIndex
    if not isinstance(raw_data.columns, pd.MultiIndex):
        raise ValueError(
            "Provider dataset must use MultiIndex columns"
        )

    # Validate existing symbol
    available_symbols = raw_data.columns.get_level_values(0)

    if symbol not in available_symbols:
        raise ValueError(
            f"Provider dataset does not contain symbol '{symbol}'"
        )

    symbol_data = raw_data[symbol]

    if not isinstance(symbol_data, pd.DataFrame):
        raise ValueError(
            f"Provider data for symbol '{symbol}'must be a DataFrame"
        )

    return symbol_data.copy()

def normalize_daily_bars(
    provider_data: pd.DataFrame,
    symbol: str,
) -> pd.DataFrame:
    if not symbol or symbol.isspace():
            raise ValueError("Symbol cannot be empty")

    if symbol != symbol.strip():
        raise ValueError(
            "Symbol cannot contain leading or trailing whitespace"
        )

    missing_columns = (
        _REQUIRED_PROVIDER_COLUMNS
        - set(provider_data.columns)
    )

    if missing_columns:
        raise ValueError(
            "Cannot normalize provider data; missing required columns: "
            + ", ".join(sorted(missing_columns))
        )

    if not isinstance(provider_data.index, pd.DatetimeIndex):
        raise ValueError(
            "Provider dataset index must be a DatetimeIndex"
        )

    session_dates = provider_data.index.strftime("%Y-%m-%d")

    canonical = pd.DataFrame(
        {
            "session_date": session_dates,
            "symbol": [symbol] * len(provider_data),
            "open": provider_data["Open"].to_numpy(copy=True),
            "high": provider_data["High"].to_numpy(copy=True),
            "low": provider_data["Low"].to_numpy(copy=True),
            "close": provider_data["Close"].to_numpy(copy=True),
            "volume": provider_data["Volume"].to_numpy(copy=True),
        }
    )

    return canonical[_CANONICAL_COLUMNS]
