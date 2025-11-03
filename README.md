
# swtor_combat_explorer

This is the SW:ToR_Combat_Explorer project. Our goal is to make the
combat log output from the Star Wars: The Old Republic MMORPG
programatically accessible from C++ for use in downstream tools, such
as real-time statistics, offline analyzers, etc.

As of now it consists of two libraries:

1. explorer (swtor_combat_explorer_lib)
2. populator (swtor_combat_populate_db_lib)

## Explorer (Parser) Library

This library ingests raw textual logfiles emitted by the SW:ToR client
and produces C++ structures translating the text into C++ types. This
abstracts away the very idiosyncratic format of the text log and into
concrete, consistent types easily used by C++ programs.

## DB Populator Library

This library is an example of using C++ types emitted by the parser
library. Specifically, this library inserts the log data extracted
from the C++ types into a SQL database. It uses libpqxx, the offical
C++ PostgreSQL front-end to populate a Postgres database. Making the
log available in this form allows for aggregate analysis and behavior
discovery that's very hard to discern from the raw log.

# Building and installing

See the [BUILDING](BUILDING.md) document.

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# What's Missing

Many things, such as:

1. asynchronous input that would allow the parser library to be used
   in a GUI
2. handling input from the live log
3. any usage/testing in Windows, which hosts the majority of SW:ToR
   clients
4. a plugin system that would allow analyzers to be plugged in at
   runtime, giving other developers a simple way perform more
   sophisticated analyzers, perhaps on a class-by-class basis
5. any kind of example (or actually useful) GUI.
6. CI/CD
7. End-to-end tests - many unit tests, but this is missing.
8. Automated database creation/deletion. Right now it all has to be
   done by hand.
9. Logging is purely textual and to stream, meaning any serious user
   would have to capture stdout/stderr and parse that (ugly and slow).

That's enough for now. Any more and I'd get too overwhelmed.

# Licensing

GNU AGPLv3
