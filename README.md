# Servant

Servant is a runtime that allows to write AF\_XDP application using eBPF. It uses uBPF in its heart for running eBPF programs.

## Dependencies

* uBPF (Link to custom version)
* libbpf


### Install uBPF

```bash
git clone ssh://git@fyro.ir:10022/fshahinfar1/uBPF.git
cd ubpf/src
make
sudo make install
```

### Installing libbpf

```bash
# Some dependencies you might need
sudo apt intall pkg_config libz-dev libelf-dev
# Getting and compiling the libbpf
git clone https://github.com/libbpf/libbpf
cd libbpf
git checkout f9f6e92458899fee5d3d6c62c645755c25dd502d
cd src
mkdir build
make all OBJDIR=.
# make install_headers DESTDIR=build OBJDIR=.
make install DESTDIR=build OBJDIR=.
sudo make install
```

**Issues with libbpf:**

```
./servant: error while loading shared libraries: libbpf.so.0: cannot open shared object file: No such file or directory
```

If there is a problem with linking the shared object, you might need to add
the libbpf.so directory to `/etc/ld.so.conf.d/libbpf.conf`. Check the path with
`whereis libbpf`.

