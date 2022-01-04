# Time Tracking Report

A console application that takes a time report of all employees and generates a monthly report.

## Usage

```
Allowed options:

Generic options:
  -h [ --help ]                produce help message
  -b [ --blacklist ] arg       list of banned lines
  -s [ --separator ] arg (=;)  separator
```

## Example

```bash
$ time_tracking_report example.csv -s , --blacklist=blacklist.txt
Banned line found warning: poison pill
Jane Doe;October 2021;4
John Doe;October 2021;13
Total time: 584
```

## TODO

* Add logging
* Add a thread pool
* Add multiple input files support
