# Cordio Updates

## How to Update Cordio

Run the `update-cordio` script. It will clone the current and 

A suite of scripts to assist updating Cordio. The suite can be run from within the source tree or moved to another location if MBED_PATH is set as an environment file or in a file called `.cordio-update` colocated with the script.

### Invocation

```
Usage: update-cordio [options] [functions]

Options:
    -h,--help     Show this help and quit

Functions:
    --ghclone     Clone the GitHub repo (old Cordio)
    --bbclone     Clone to BitBucket repos (new Cordio)
    --clean       Clean up cloned repos
    --restructure Restructure the new Cordio to match how the old Cordio was brought into mbed OS
    --make-diffs  Create diffs between the old Cordio and mbed OS
    --summary     Create a summary of the cloned repos
    --replace     Replace the Cordio in MBED_ROOT with the new Cordio

NB: By default, all of the above functions are implicitly enabled and will run in order. Explicitly enabling a function
implicitly disables the others unless they are explicitly enabled as well. The order of execution can not be affected by
command-line arguments.

Configuration file:
If present, configuration will be read from a file called $CONFIG_FILE in the same location as the script. The following
variables can be set there:

    MBED_ROOT           Path to a local copy of the mbed OS Git repo, main branch. The default is to look for a .git
                        directory that is higher than the current directory.
    WORKDIR             Directory to store work in. Defaults to a random temporary directory which is deleted if the
                        script exits for any reason (user-supplied directories are not deleted).
    GITHUB_USERNAME     Username to use when pulling Git repo. Defaults to current user's name.
    GITHUB_PASSWORD     Password to use when pulling Git repo. Required if --ghclone is enabled.
    BITBUCKET_USERNAME  Username to use when pulling BitBucket repos. Defaults to current user's name.
    BITBUCKET_PASSWORD  Password to use when pulling BitBucket repos. Required if --bbclone is enabled.

Environment:
If no configuration file is present the above variables will be read from the environment. Additionally, the following
can be set:

    EDITOR              Set to an executable program. Defaults to $DEFAULT_EDITOR.
    dos2unix            Should be executable via PATH.
```