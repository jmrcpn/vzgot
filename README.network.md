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

Persistent Bridge Configuration (NetworkManager / nmcli)

For systems running systemd with NetworkManager, here is
how to persistently set up Scenario B so that it survives reboots.

⚠️ SSH Safety Warning

If you are configuring this remotely over SSH via eth0,
swapping the IP to the bridge will momentarily drop your connection.
To prevent getting permanently locked out, run these commands
together in a single script or screen session.

Step-by-Step Setup:

Identify your network interfaces:
Find the exact name of your physical network card (we will use eth0
in this example):

ip link show


Create the persistent bridge interface:
This creates the bridge profile in NetworkManager:

sudo nmcli connection add type bridge con-name vz-br0 ifname vz-br0


Configure the Bridge IP addresses:
Assign both the native 192.0.2.1/32 address and your Host's
physical LAN IP (e.g., 192.168.10.32/24) to the bridge.
Also, set your local router as the gateway for the bridge:

sudo nmcli connection modify vz-br0 ipv4.method manual \
  ipv4.addresses "192.0.2.1/32,192.168.10.32/24" \
  ipv4.gateway "192.168.10.1" \
  ipv4.dns "1.1.1.1,8.8.8.8"


Bind your physical interface to the bridge:
Create a "slave" profile for your physical card eth0 and bind
it to vz-br0:

sudo nmcli connection add type bridge-slave con-name vz-br0-slave ifname eth0 master vz-br0


Apply changes (The "Point of No Return" over SSH):
Delete your old physical connection profile (usually called Wired connection 1 or eth0)
and bring up the bridge. Running these commands as a one-liner ensures
NetworkManager switches the interfaces instantly:

sudo nmcli connection down eth0 && \
sudo nmcli connection del eth0 && \
sudo nmcli connection up vz-br0 && \
sudo nmcli connection up vz-br0-slave


Once completed, your Host is reachable at 192.168.10.32 via
the vz-br0 bridge, and your containers can now natively join
your local network.
