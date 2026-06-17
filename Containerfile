FROM opensuse/leap:15.6

# Build deps + HDF5
RUN zypper -n refresh && \
    zypper -n install gcc make hdf5-devel

# PicoScope SDK (installs libps5000a + headers under /opt/picoscope)
RUN rpmkeys --import http://labs.picotech.com/repomd.xml.key && \
    zypper -n addrepo https://labs.picotech.com/picoscope7/rpm/ picoscope && \
    zypper -n --gpg-auto-import-keys refresh && \
    zypper -n install libps5000a && \
    zypper clean -a

WORKDIR /build
COPY Makefile rapid_block_mode.c ./

RUN make rapid_block_mode

CMD ["./rapid_block_mode"]
