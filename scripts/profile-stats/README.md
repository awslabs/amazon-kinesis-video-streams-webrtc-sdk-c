# Profile Statistics Aggregator

Aggregates PROFILE log metrics from KVS WebRTC logs

## Requirements

- Python 3.13

## Setup

```bash
cd scripts/profile-stats
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

## Usage

```bash
# Single file
python3 ./scripts/profile-stats/aggregate.py /path/to/log.txt

# Multiple files
python3 ./scripts/profile-stats/aggregate.py /path/to/log1.txt /path/to/log2.txt

# All logs in directory
python3 ./scripts/profile-stats/aggregate.py ./build/kvsFileLogFilter.*

# Filter metrics with minimum count
MIN_COUNT=3 python3 ./scripts/profile-stats/aggregate.py ./build/kvsFileLogFilter.*
```

## Environment Variables

- `MIN_COUNT`: Minimum sample count to include metric in the table (default: 1)

## Output

Shows Avg, Min, P50, P90, Max, and Count for each metric across all provided log files.
