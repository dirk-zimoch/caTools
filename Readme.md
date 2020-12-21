# Overview

*catools* provides easy to use command line tools to work with ChannelAccess channels.

# Usage

*catools* comes as a single binary and provides following Channel Access tools:

- `caget`: Reads PV values(s). Usage:

  ```
  caget [flags] <pv> [<pv> ...]
  ```

- `cagets`: First processes the PV(s), then reads the value(s). Usage:

  ```
  cagets [flags] <pv> [<pv> ...]
  ```

- `camon`: Monitors the PV(s). Usage:

  ```
  camon [flags] <pv> [<pv> ...]
  ```

- `caput`: Writes value(s) to the PV(s), waits until processing finishes and returns updated value(s). Usage:

  ```
  caput [flags] <pv> <value> [<pv> <value> ...]
  ```

- `caputq`: Writes value(s) to PV(s), but does not wait for the processing to finish. Does not have any output (except if an error occurs). Usage:

  ```
  caputq [flags] <pv> <value> [<pv> <value> ...]
  ```

- `cado`: Processes the PV(s) and exits. Does not have any output (except if an error occurs). Usage:

  ```
  cado [flags] <pv> [<pv> ...]
  ```

- `cawait`: Monitors the PV(s), but only displays the values when they match the provided conditions. Usage:

  ```
  cawait [flags] <pv> <condition> [<pv> <condition> ...]
  ```

The conditions are specified as a string containing the operator together with the values.
Following operators are supported:  `>, <, <=, >=, ==, !=, ==A...B, !=A...B`.


- `cainfo`: Displays detailed information about the provided records. Usage:

  ```
  cainfo [flags] <pv> [<pv> ...]
  ```


Detailed help for each tool is available with `-h` flag. Eg. run `caget -h` to get help for caget tool.

## Default Parsing and Formatting
If no flags are set catools will allways try to parse the input as number(s). If number(s) could not be found the input will be interpreted as string where possible (enum and char field types). Similarly catools will check for char arrays if the output can be printed as ascii. I this case it will be printed as string or a n array of numbers otherwise. To force parsing and formating as strings use `-s` flag, and use `-num`, `-bin`, `-hex` or `-oct` to force numeric parsing and formatting.

## cawait Examples

The `cawait` command can be used in shell scripts that need to wait until EPICS channels reach certain values without the need to poll the value repeatedly. It will exit upon meeting the user specified condition. Here are a few examples:

__Usage:__
```
camon CHANNEL 'condition' [CHANNEL2 'condition' ...]
```

Valid conditions are:
* `<n`
* `>n`
* `<=n`
* `>=n`
* `==x` or `=x` or simply `x`
* `a...b` meaning `>=a` and `<= b`
* `!` means "not" and can can be prefixed to any condition

with numbers `n, a, b` and numbers or strings `x`.

Note that `< >` and `!` need to be escaped from the shell, so best use single quotes like `'condition'`



If you wait for multiple channels at the same time like this `cawait XXX '<1' YYY '>5'` it will wait until any condition matches, thus it implements a logical OR.

If you need an AND it is often sufficient to wait sequentially: `cawait XXX '<1'; cawait YYY '>5'`

The command does not wait at all if the condition was already true at the beginning.


__Examples:__
- Exit when value of caTools-TEST:AI is smaller than 5.

  ```
  cawait caTools-TEST:AI '<5'
  ```

- Exit when value of caTools-TEST:AI is greater or equal to 5.

  ```
  cawait caTools-TEST:AI '>=5'
  ```

- Exit when value of caTools-TEST:AI is not 5.

  ```
  cawait caTools-TEST:AI '!5'
  ```

- Exit when value of caTools-TEST:AI is between 5 and 10.

  ```
  cawait caTools-TEST:AI '5...10'
  ```

- Exit when value of caTools-TEST:AI is not between 5 and 10.

  ```
  cawait caTools-TEST:AI '!5...10'
  ```

- Exit when the string value of TEST_MBBO is not "50.00 Hz"

  ```
  cawait TEST_MBBO '!50.00 Hz'
  ```

- state (e.g. string) is reached 
  ```
  cawait CHANNEL 'DONE'
  ```

- state is not active any more 
  ```
  cawait CHANNEL '!Moving'
  ```

# Development
## Build
The tools are structured as an EPICS application.

1. Clone this repository in your local folder ( referenced as the `<top>` folder).
1. (Optional) Amend `<top>/configure/RELEASE` to match your site specifics.
  - i.e. uncomment and adapt EPICS_BASE=/usr/local/epics/base-3.14.12
1. (Optional) Amend `<top>/configure/CONFIG_SITE` to match your site specifics.
1. Move to `<top>` folder and run `make`
1. Find the executable in `<top>/bin/<architecture>/caTools`
1. Find symbolic links for all the tools in `<top>/bin/<architecture>/` folder.


# Packaging

While building the package the code with the respective version is downloaded from the Git repository, build and then put into the package.

Therefor make sure that you committed and pushed your changes to the git repository for building the RPM!


### RHEL7

The RPM package can be build on a __RHEL7__ system 

```bash
rpmbuild -ba epics-ext-caTools.spec
```

Without a special rpmbuild configuraiton the resulting package will end up in `~/rpmbuild/RPMS` .


### SL6

The RPM package can be build on a __SL6__ system 

```bash
rpmbuild -ba epics-ext-caTools.spec
```

Without a special rpmbuild configuraiton the resulting package will end up in `~/rpmbuild/RPMS` .

