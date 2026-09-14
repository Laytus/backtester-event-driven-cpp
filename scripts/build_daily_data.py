import argparse
import re
from datetime import date
from pathlib import Path

from backtester_data.pipeline import run_daily_data_pipeline


def _parse_date(value: str) -> date:
    if re.fullmatch(r"\d{4}-\d{2}-\d{2}", value) is None:
        raise argparse.ArgumentTypeError(
            "Date must use YYYY-MM-DD format"
        )

    try:
        return date.fromisoformat(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            f"Invalid calendar date: {value}"
        ) from error


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Download Yahoo Finance daily data and build "
            "canonical CSV data for the C++ backtester."
        )
    )

    parser.add_argument(
        "--symbol",
        required=True,
        help="Yahoo Finance symbol, for example SPY.",
    )

    parser.add_argument(
        "--start",
        required=True,
        type=_parse_date,
        help="Inclusive start date in YYYY-MM-DD format.",
    )

    parser.add_argument(
        "--end",
        required=True,
        type=_parse_date,
        help="Exclusive end date in YYYY-MM-DD format.",
    )

    parser.add_argument(
        "--raw-dir",
        type=Path,
        default=Path("data/raw"),
        help="Directory for raw provider data and metadata.",
    )

    parser.add_argument(
        "--canonical-dir",
        type=Path,
        default=Path("data/canonical"),
        help="Directory for canonical CSV data.",
    )

    return parser


def main(
    argv: list[str] | None = None,
) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    base_name = (
        f"{args.symbol}_"
        f"{args.start.isoformat()}_"
        f"{args.end.isoformat()}"
    )

    raw_path = (
        args.raw_dir
        / f"{base_name}.csv"
    )

    metadata_path = (
        args.raw_dir
        / f"{base_name}.metadata.json"
    )

    canonical_path = (
        args.canonical_dir
        / f"{base_name}.csv"
    )

    canonical_data = run_daily_data_pipeline(
        symbol=args.symbol,
        start_date=args.start,
        end_date=args.end,
        raw_path=raw_path,
        metadata_path=metadata_path,
        canonical_path=canonical_path,
    )

    print(
        f"Built {len(canonical_data)} canonical daily bars."
    )
    print(f"Raw data: {raw_path}")
    print(f"Metadata: {metadata_path}")
    print(f"Canonical data: {canonical_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())