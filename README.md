# Marhaba

RSS 2.0 aggregator written in C. Fetches feeds from sources stored in a SQLite database and inserts articles as plain text.

## Components

- **aggregator** — fetches RSS feeds and stores articles in the database
- **publisher** — executes a read-only SELECT query on the database and returns JSON
- **bot** — Perl script for RSS feed auto-discovery from a list of URLs

## Dependencies

### System libraries

| Library | Debian/Ubuntu package |
|---|---|
| libcurl | `libcurl4-openssl-dev` |
| libsqlite3 | `libsqlite3-dev` |
| libmxml | `libmxml-dev` |
| libtidy | `libtidy-dev` |
| json-c | `libjson-c-dev` |

```sh
apt install libcurl4-openssl-dev libsqlite3-dev libmxml-dev libtidy-dev libjson-c-dev
```

### Perl modules (bot only)

```sh
cpan LWP XML::Feed Text::Trim
```

## Build

```sh
cd src
make
make install
make clean
```

Binaries are installed to `bin/`.

## Database setup

Copy `db/example.sqlite` to `db/marhaba.sqlite` and populate the `sources` table:

```sql
INSERT INTO sources (url, name) VALUES ('https://example.com/feed.rss', 'Example');
```

## Usage

```sh
bin/aggregator
bin/publisher "SELECT * FROM news ORDER BY dbDate DESC LIMIT 10"
```

`publisher` only accepts SELECT queries.

## Bot

```sh
cd bot
./bot.pl
```

Reads URLs from `input.csv` (one per line), discovers RSS/Atom feeds via auto-discovery, and appends results to `output.csv`.
