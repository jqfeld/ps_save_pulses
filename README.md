# Build
## OpenSUSE leap
Install build dependencies:
```sh
zypper -n refresh && \
zypper -n install gcc make hdf5-devel

# PicoScope SDK (installs libps5000a + headers under /opt/picoscope)
rpmkeys --import http://labs.picotech.com/repomd.xml.key && \
    zypper -n addrepo https://labs.picotech.com/picoscope7/rpm/ picoscope && \
    zypper -n --gpg-auto-import-keys refresh && \
    zypper -n install libps5000a && \
    zypper clean -a

```

Then build by running make:
```sh
make rapid_block_mode
```

## Testing the build on non OpenSUSE systems
```sh
podman build -t rapidblock .
```
