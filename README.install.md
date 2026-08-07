# vzgot Container Installation & Operations Guide

This document provides a step-by-step guide for installing the `vzgot` daemon on a host server, provisioning containers, managing their lifecycle, and performing container migrations.

---

## 1. Installing the `vzgot` Daemon

Before provisioning containers, you must install the `vzgot` daemon on the host server. Download and extract the latest release tarball from GitHub:

```bash
wget https://github.com/your-repo/vzgot/releases/download/vX.Y.Z/vzgot-X.Y.Z.tar.gz
tar -xzvf vzgot-X.Y.Z.tar.gz
cd vzgot-X.Y.Z
```

Depending on your host operating system and deployment preferences, choose one of the three installation methods below:

### Method A: Direct System Installation (`make install`)
For a direct installation from source code, compile and install the binaries and configuration files directly onto the host system:

```bash
# Build and install directly onto the host
sudo make install
```
This target installs the `vzgot` binary, default configuration files under `/etc/vzgot/`, and system init/service scripts.

---

### Method B: Building and Installing RPM Packages (`make dorpm`)
For RedHat, Fedora, RHEL, CentOS, or SUSE-based systems, `vzgot` provides a dedicated target to build native RPM packages:

```bash
# Generate the RPM package
make dorpm

# Install the generated RPM package
sudo rpm -ivh vzgot-*.rpm
# OR using dnf
sudo dnf install ./vzgot-*.rpm
```
This method integrates directly with your system's RPM database, simplifying future updates and clean package removals.

---

### Method C: Debian / Ubuntu Package Installation (`debian`)
For Debian, Ubuntu, and derivative distributions, `vzgot` includes packaging support located in the `debian/` directory:

```bash
# Build the Debian package (.deb)
dpkg-buildpackage -b -uc -us
# OR if using the Makefile wrapper target:
make dodeb

# Install the generated .deb package
sudo dpkg -i ../vzgot_*.deb
# Resolve any missing runtime dependencies if required
sudo apt-get install -f
```

---

## 2. Template Configuration (`/etc/vzgot/vzgot_list`)

The installation process relies on template mappings defined in `/etc/vzgot/vzgot_list`.

Container templates can be obtained from official repositories such as:
> **Linux Containers Image Repository:** https://images.linuxcontainers.org/

Before creating a container, ensure `/etc/vzgot/vzgot_list` contains the appropriate mapping for your container name, template source, distribution, and target architecture.

### Example `/etc/vzgot/vzgot_list`

```text
#cont_name    template_name         distribution    architecture
alpine        alpine-3.24             alpine            x86_64
debian        debian-202606           debian            x86_64
dummy1        voidlinux-260714       void              x86_64
fedora        fedora-20260714         fedora            x86_64
gentoo        gentoo-260715           gentoo            x86_64
osukiss       vzgot-ok-26.06          ok-1              x86_64
ubuntu        ubuntu-20260713         ubuntu            x86_64
void          voidlinux-260714       void              x86_64
```

---

## 3. Creating a Container

To provision a container, use the `vzgot create` command. This creates the container filesystem, assigns its IP address, and configures host `iptables` protection.

### Syntax
```bash
vzgot create <cont_name> <IP_address>
# OR using an FQDN
vzgot create <cont_name.domain.tld>
```

### Example
To create a container named `dummy1` with IP `192.168.254.215`:

```bash
vzgot create dummy1 192.168.254.215
```

Upon successful installation, `vzgot` will output the following prompt:

```text
dummy1 is now properly installed; to start it,
type: "vzgot boot dummy1".
```

---

## 4. Starting the Container

To boot the newly created container, run:

```bash
vzgot boot dummy1
```

---

## 5. Accessing and Provisioning the Container

### Standard SSH Access
Once booted, you can access the container using standard SSH:

```bash
ssh user@192.168.254.215
```

### Console Access via Host (`vzgot enter`)
Some distribution templates do not include a pre-installed SSH daemon. If SSH is unavailable, access the container shell directly from the host hardware using:

```bash
vzgot enter dummy1
```

Once inside the container, install and configure an SSH server using the distribution's package manager.

---

## 6. Enabling Automatic Boot on Host Startup

Once your container verification checklist is complete, configure the container to start automatically whenever the Host hardware reboots:

```bash
vzgot onboot dummy1
```

---

## 7. Checking Container Status

To monitor active containers, run:

```bash
vzgot online
```

### Sample Output
```text
alpine:        UP for   0 days  0:07:36,  0 Users, load average:   0.00   0.00   0.00,  1/5 468
debian:        UP for   0 days  0:07:33,  0 Users, load average:   0.00   0.00   0.00,  1/9 67
dummy1:        UP for   0 days  0:00:26,  0 Users, load average:   0.00   0.00   0.00,  1/15 54
fedora:        UP for   0 days  0:07:27,  0 Users, load average:   0.00   0.00   0.00,  1/15 107
gentoo:        UP for   0 days  0:07:24,  0 Users, load average:   0.00   0.00   0.00,  1/4 1017
osukiss:       UP for   0 days  0:07:19,  0 Users, load average:   0.00   0.00   0.00,  1/14 388
ubuntu:        UP for   0 days  0:07:18,  0 Users, load average:   0.00   0.00   0.00,  1/13 148
void:          UP for   0 days  0:07:16,  0 Users, load average:   0.00   0.00   0.00,  1/15 53
```

---

## 8. Migrating Containers Between Hosts

`vzgot` allows you to move a running container from one host to another.

To pull a container (e.g., `dummy1`) from a source host (`host1`) to the target host (`host2`), run the following command **on `host2`**:

```bash
vzgot movefrom host1 dummy1
```
