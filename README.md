# vzgot

A lightweight, bare-metal-like system containerization 
and supervisor utility written in pure C.

---

## Why vzgot? (The Philosophy)

**vzgot** is a modern, lightweight system container engine built directly on upstream Linux kernel technologies. Designed for persistent, full-OS environments, it combines 15+ years of operational container experience with state-of-the-art primitives like cgroup v2, native network namespaces, and strict AppArmor security confinement.

Executed in zero-dependency, high-performance C, **vzgot** bridges the gap between historical system container reliability and modern Linux kernel capabilities.

### Key Advantages

* **System Containers vs. Application Containers:** Unlike runtimes designed strictly for single-process microservices, `vzgot` boots a complete, independent OS userland. It handles standard system initialization (Systemd, SysVinit, OpenRC) flawlessly.
  
* **Full Root Independence:** Inside the container, users act as true `root`. They can manage their environment, services, and local configurations in complete autonomy, just like on a dedicated physical server.
  
* **15+ Years of Production-Proven Maturity:** Refined and operated continuously in production environments since 2009, `vzgot` is strictly engineered for 24/7 non-stop operations.
  
* **Zero Runtime Bloat:** Written in clean C with minimal local shell scripts. No heavy background daemons, no complex storage layers, and no systemd dependencies required on the host.

---

## Performance & Resource Isolation in Action

The true power of `vzgot` lies in its precise cgroup orchestration. It ensures that heavy neighbor workloads never compromise critical host operations or other environments.

> **Real-World Proof of Concept:**
> There is a distinct technical satisfaction in watching three separate containers violently compiling massive software suites (such as a Beyond Linux From Scratch project) over multiple iterative loops. Even with the cluster driving a peak system load of 160 on a 28-core processor, a developer can seamlessly work inside a fourth container without experiencing a single millisecond of noticeable input lag or latency.

---

## Core Capabilities

* **Hardware-Like Isolation:** A container is functionally indistinguishable from physical hardware. It possesses all standard server attributes (runs its own `iptables`, databases, cron jobs, etc.).
  
* **Strict Namespace Control:** Containers are tightly constrained and cannot interfere with host hardware or other containers by default.
  
* **Selective Hardware Passthrough:** When required, the host sysadmin can explicitly grant a container access to specific physical devices.
  
* **Predictable Networking:** Supports standard Linux network architectures, allowing containers to be attached to Layer 2 (Bridge) or Layer 3 (Routed) configurations.

---

## Extensibility & OS Customization

The shell script component of `vzgot` is specifically designed to let you fine-tune and customize the deployment of OS templates inside your containers.

**We highly encourage community contributions!** If you create or adapt a boot/setup script for a specific layout, please submit your `vzgot.fboot.my_very_own_os` script. We will gladly integrate it into the official release to continuously expand `vzgot`'s ecosystem and adaptability.

---

## Further Documentation

For deep dives into specific topics, please refer to our dedicated sub-documentation guides:

* [Installation & Packaging Guide](README.install.md)
* [System Administrator Manual](README.manual.md)
* [Advanced Network Configurations](README.network.md)
