vzgot Network Configuration Guide

This document explains how to configure networking for vzgot
containers. By design, vzgot relies on a dedicated network
bridge interface on the host system named vz-br0.

1. Prerequisite: Enable Kernel IP Forwarding

Before configuring any container network, the Linux kernel
on the Host must be allowed to forward traffic between interfaces.

Check your current status:

sysctl net.ipv4.ip_forward


If it returns 0, enable it immediately:

sudo sysctl -w net.ipv4.ip_forward=1


To make this change permanent, ensure the following line is
uncommented in /etc/sysctl.conf or a dedicated file under
/etc/sysctl.d/:

net.ipv4.ip_forward = 1


2. The Base Architecture (vz-br0)

During installation, a virtual bridge interface named vz-br0
is defined. When this interface is activated (via NetworkManager,
ifupdown, or system network scripts), it is automatically
assigned the IP address 192.0.2.1.

The Container Perspective: From inside the container,
192.0.2.1 acts as its default gateway (default route) in
all networking scenarios.

The Host Perspective: As soon as a container is booted,
the Host can natively ping the container's IP address
directly through this bridge.

Depending on your network infrastructure, you must choose
between two deployment scenarios to make the container
accessible from the rest of your network:

Scenario A: Routed Architecture (Layer 3)

Choose this method if your Host is already acting as a router,
or if you run a routing daemon (like BIRD, Quagga/FRR) using
protocols such as RIP, OSPF, or BGP.

How it works:

The container can be assigned a local private IP or a
public IP address.

The Host receives packets destined for the container and
routes them internally to the vz-br0 interface.

Route Propagation: The Host must actively announce this
new route to its network peers. Through your routing daemon
(e.g., BIRD), the Host dynamically propagates the container's
IP or subnet prefix, automatically informing upstream routers
that this Host is the valid next-hop for that container.

Advantage: Extremely clean, no modification needed on the
Host's physical interface configurations.

Scenario B: Bridged Architecture (Layer 2)

Choose this method if you do not have a routed network infrastructure
and want your containers to reside directly on your local LAN
(sharing the same subnet as your Host, e.g., 192.168.10.0/24).

How it works:

Because you cannot have the same subnet separated across two
independent interfaces, you must merge your physical layout into
the bridge.

Strip the IP from the physical interface: Remove the LAN IP
(e.g., 192.168.10.32) from your standard network card
(e.g., eth0).

Enslave the interface: Bind eth0 to become a port
(slave) of vz-br0.

Assign IPs to the Bridge: Add your Host's original LAN IP
as a secondary address on vz-br0, right alongside the
native 192.0.2.1 address.

Example Topology:

Host Physical Interface (eth0): No IP address assigned.

Host Bridge Interface (vz-br0):

Main IP: 192.0.2.1/32

Secondary IP (Your LAN IP): 192.168.10.32/24

Container IP: 192.168.10.56/24 (Gateway: 192.0.2.1)

With this setup, the bridge acts like a network switch.
Any device on your local LAN can now ping and communicate
with your container directly, while the container continues
to use 192.0.2.1 as its universal default gateway.
