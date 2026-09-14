from datetime import UTC, date, datetime
from pathlib import Path

import pandas as pd

from backtester_data.acquisition import (
    build_acquisition_metadata,
    download_daily_data,
)
from backtester_data.normalization import (
    normalize_daily_bars,
    select_symbol,
)
from backtester_data.persistence import (
    write_canonical_data,
    write_metadata,
    write_raw_data,
)
from backtester_data.validation import (
    validate_canonical_data,
    validate_provider_data,
)


def _utc_now() -> datetime:
    return datetime.now(UTC)

def run_daily_data_pipeline(
    symbol: str,
    start_date: date,
    end_date: date,
    raw_path: Path,
    metadata_path: Path,
    canonical_path: Path,
) -> pd.DataFrame:
    raw_data = download_daily_data(
        symbol,
        start_date,
        end_date,
    )

    downloaded_at_utc = _utc_now()

    write_raw_data(
        raw_data,
        raw_path,
    )

    metadata = build_acquisition_metadata(
        symbol=symbol,
        start_date=start_date,
        end_date=end_date,
        raw_data=raw_data,
        downloaded_at_utc=downloaded_at_utc,
    )

    write_metadata(
        metadata,
        metadata_path,
    )

    validate_provider_data(
        raw_data,
        symbol,
    )

    provider_data = select_symbol(
        raw_data,
        symbol,
    )

    canonical_data = normalize_daily_bars(
        provider_data,
        symbol,
    )

    validate_canonical_data(
        canonical_data,
    )

    write_canonical_data(
        canonical_data,
        canonical_path,
    )

    return canonical_data