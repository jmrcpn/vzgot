# Copyright 2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

DESCRIPTION="Container engine for Linux"
HOMEPAGE="https://github.com/jmrcpn/vzgot"
SRC_URI="${P}.tar.gz"

RESTRICT="fetch"

LICENSE="GPL-2+"
SLOT="0"
KEYWORDS="~amd64"

# Dépendances d'exécution (bc-gh de Gavin Howard)
RDEPEND="
	sci-calculators/bc-gh
	sys-libs/libcap
"
# Dépendances de compilation
DEPEND="${RDEPEND}"

S="${WORKDIR}/${P}"

src_compile() {
	emake
}

src_install() {
	emake DESTDIR="${D}" install

	keepdir /var/lib/vzgot/etc/ssh/access
	keepdir /var/lib/vzgot/etc/ssh/server

	rm -rf "${D}/var/run"
}
