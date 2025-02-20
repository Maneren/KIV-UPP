# Meteodata processing

## Building

```sh
mkdir -p build
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build
```

or

```sh
just configure_release build
```

With the [just](https://github.com/casey/just) command runner.

### Build options

`PERF_TEST_MACRO` - if defined (in CMakeLists.txt), build the binary in
performance testing mode (see below)

## Running

```sh
build/meteo --parallel|--serial path/to/stanice.csv path/to/mereni.csv
```

## Documentation

### Analysis

Generally I first tried to write the serial version in reasonably optimized
manner to have good baseline for comparison. Also I structured the code in
a way, that leans heavily into ranges and higher-order-functions (HOF) (map,
reduce, ...) that besides being clean and fast by themselves also allow for
a straightforward parallelization later.

Furthermore I split the program into four parts:

- data loading
- preprocessing
- outliers detection
- rendering the visualization

They have roughly the following flow:

```mermaid
stateDiagram-v2
    direction LR
    state PFork <<fork>>
    state PJoin <<join>>
    DL : Data loading
    [*] --> Stations
    state DL {
        Stations --> Measurements
        Measurements --> PFork
    }
    state OFork <<fork>>
    state OJoin <<join>>
    state Preprocessing {
        PS1 : Station 1
        PS2 : Station 2
        PS3 : Station N
        PFork --> PS1
        PFork --> PS2
        PFork --> PS3
        PS1 --> PJoin
        PS2 --> PJoin
        PS3 --> PJoin
        PJoin --> Removal
        Removal --> Averages
    }
    state RFork <<fork>>
    state RJoin <<join>>
    Outliers : Outliers detection
    state Outliers {
        Averages --> OFork
        OS1 : Station 1
        OS2 : Station 2
        OS3 : Station N
        OFork --> OS1
        OFork --> OS2
        OFork --> OS3
        OS1 --> OJoin
        OS2 --> OJoin
        OS3 --> OJoin
        OJoin --> MinMax
    }
    Rendering : SVG rendering
    state Rendering {
      MinMax --> RFork
      RM1 : Month 1
      RM2 : Month 2
      RM3 : Month N
      RS1 : SVG 1
      RS2 : SVG 2
      RS3 : SVG N
      RFork --> RM1
      RFork --> RM2
      RFork --> RM3
      RM1 --> RS1
      RM2 --> RS2
      RM3 --> RS3
      RS1 --> RJoin
      RS2 --> RJoin
      RS3 --> RJoin
    }
    RJoin --> [*]
```

#### Data loading

Simplest part of the program, just reads the file line-by-line, splits each line
and saves the data to a vector or Stations structs. Reading one file is very
hard to parallelize, since it involves a lot of very small operations using
temporary buffers, that would have to be copied into the threads making them
quite costly. Due to that, I left it serial.

#### Preprocessing

Preprocessing basically needs to run a check for every station and then remove
the stations that don't pass the check. This allows for all of the stations to
be checked in parallel but the (relatively fast) removal is better to be done
later serially to prevent excessive locking.

#### Outliers detection

Here we have to first calculate the average temperatures and then use them to
find outliers. Both are in theory parallelizable (independent tasks for each
station), however the averaging is fast enough serially for the size of the
sample data, making it not worth it to parallelize. On the other hand, the
outlier detection itself is more complex and thus fit for parallelization.

#### SVG rendering

Again this part consists of a bunch of independent task, although this time for
each month rather than each station. It is also very beneficial to parallelize
the file access, since the compute part can run, while another thread is waiting
for a file to finish writing to the drive.

### Implementation

To simplify the parallel implementation I structured the serial code already
into easily paralelizable functional range-based paradigm and subsequentially
created abstractions building upon these principles.

As an example in the serial version of preprocessing I wrote

```cpp
for (auto [i, valid] :
    std::views::transform(stations, valid_station) |
        views::enumerate |
        views::reverse
) {
  if (!valid) {
    swap_remove(stations, i);
  }
}
```

so in the parallel version I just swapped the range adapter for a parallel
implementation[^standard]

```cpp
for (auto [i, valid] :
    parallel_transform(stations, valid_station) |
        std::views::enumerate |
        std::views::reverse
) {
  if (!valid.get()) {
    swap_remove(stations, i);
  }
}
```

The overall code structure is nearly identical, but it runs in parallel and
around 2.1 times faster[^laptop].

#### Threadpool

After creating the parallel abstraction, I noticed, that they spawn hundreds of
threads usually for quite small tasks, making the thread creation and deletion a
major part of the runtime. So I implemented a very basic threadpool and altered
the parallel abstractions slightly to use it instead of `std::thread`. This
resulted in major speedups, making the snippet shown above nearly 3 times
faster than the serial version[^laptop].

#### Performace testing mode

To improve the precision of the time measurements, I added a `PERF_TEST_MACRO`
compile time option, that adds code to run individual parts of the code
multiple times and print the average time.

### Results

For total runtime, I decided to omitt the data loading part since it takes
between 1200 and 1500 milliseconds, while all of the "interesting" processing
takes in total less than 20 milliseconds.

Test devices were:

- laptop: Intel i7-1165G7 (4 cores/8 threads)
- desktop: AMD Ryzen 7 5800X (8 cores/16 threads)

| part          | device  | serial $[μs]$ | parallel $[μs]$ | speedup |
| ------------- | ------- | ------------: | --------------: | ------: |
| preprocessing | laptop  |          6063 |            2200 |    2.75 |
|               | desktop |          4520 |            3261 |    1.40 |
| outliers      | laptop  |          7068 |            2900 |    2.44 |
|               | desktop |          5184 |            2885 |    1.80 |
| rendering     | laptop  |          6669 |            1947 |    3.43 |
|               | desktop |          1412 |             422 |    3.35 |

Interestingly, while the desktop was faster overall the performace difference
varies wildly between the parts of the program. Also the speedups from
parallelization are overall slightly more noticeable on the laptop with fewer
cores, making me think it has something to do with scheduling overhead or
different cache structure.

[^standard]:
    Custom made, because the standard library parallel algoritms have poor
    support across compilers.

[^laptop]: On the laptop with Intel i7-1165G7 (4 cores/8 threads)
