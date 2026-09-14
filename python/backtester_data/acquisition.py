from datetime import date, datetime, timedelta
from importlib.metadata import version as package_version
from typing import TypedDict

import pandas as pd
import yfinance as yf


class _DownloadKwargs(TypedDict):
    tickers: list[str]
    start: str
    end: str
    interval: str
    group_by: str
    auto_adjust: bool
    back_adjust: bool
    repair: bool
    keepna: bool
    actions: bool
    threads: bool
    ignore_tz: bool
    prepost: bool
    rounding: bool
    progress: bool
    multi_level_index: bool

def _validate_request(
    symbol: str,
    start_date: date,
    end_date: date,
) -> None:
    if not symbol or symbol.isspace():
        raise ValueError("Symbol cannot be empty")

    if symbol != symbol.strip():
        raise ValueError(
            "Symbol cannot contain leading or trailing whitespace"
        )

    if start_date >= end_date:
        raise ValueError(
            "Start date must be earlier than end date"
        )

def _build_download_kwargs(
    symbol: str,
    start_date: date,
    end_date: date,
) -> _DownloadKwargs:
    return {
        "tickers": [symbol],
        "start": start_date.isoformat(),
        "end": end_date.isoformat(),
        "interval": "1d",
        "group_by": "ticker",
        "auto_adjust": False,
        "back_adjust": False,
        "repair": False,
        "keepna": True,
        "actions": True,
        "threads": False,
        "ignore_tz": True,
        "prepost": False,
        "rounding": False,
        "progress": False,
        "multi_level_index": True,
    }

def download_daily_data(
    symbol: str,
    start_date: date,
    end_date: date,
) -> pd.DataFrame:
    _validate_request(symbol, start_date, end_date)

    kwargs = _build_download_kwargs(symbol, start_date, end_date)

    try:
        raw_data = yf.download(**kwargs)
    except Exception as error:
        raise RuntimeError(
            f"Failed to download Yahoo Finance data for '{symbol}'"
        ) from error

    if raw_data is None:
        raise RuntimeError(
            f"Yahoo Finance returned no dataset for '{symbol}'"
        )

    return raw_data

def build_acquisition_metadata(
    symbol: str,
    start_date: date,
    end_date: date,
    raw_data: pd.DataFrame,
    downloaded_at_utc: datetime,
) -> dict[str, object]:
    _validate_request(symbol, start_date, end_date)

    if (
        downloaded_at_utc.tzinfo is None
        or downloaded_at_utc.utcoffset() is None
    ):
        raise ValueError(
            "Download timestamp must be timezone-aware"
        )

    if downloaded_at_utc.utcoffset() != timedelta(0):
        raise ValueError(
            "Download timestamp must be in UTC"
        )

    kwargs = _build_download_kwargs(symbol, start_date, end_date)

    return {
        "provider": "yahoo_finance",
        "library": {
            "name": "yfinance",
            "version": package_version("yfinance"),
        },
        "downloaded_at_utc": downloaded_at_utc.isoformat(),
        "request": {
            "symbols": [symbol],
            "selected_symbol": symbol,
            "start_date": start_date.isoformat(),
            "end_date": end_date.isoformat(),
            "interval": kwargs["interval"],
            "group_by": kwargs["group_by"],
            "auto_adjust": kwargs["auto_adjust"],
            "back_adjust": kwargs["back_adjust"],
            "repair": kwargs["repair"],
            "keepna": kwargs["keepna"],
            "actions": kwargs["actions"],
            "threads": kwargs["threads"],
            "ignore_tz": kwargs["ignore_tz"],
            "prepost": kwargs["prepost"],
            "rounding": kwargs["rounding"],
            "progress": kwargs["progress"],
            "multi_level_index": kwargs["multi_level_index"],
        },
        "result": {
            "raw_row_count": len(raw_data)
        },
    }
