#! /bin/sh
#-------------------------------------------------
#return the current Linux distribution ID
#-------------------------------------------------
DIST_ID="ukn.000"

if [ -f /etc/os-release ]; then
  . /etc/os-release

  case "${ID}" in
    "alpine"			)
      DIST_ID="${ID}${VERSION_ID}"
      ;;
    "debian"			)
      DIST_ID="deb${VERSION_ID}"
      ;;
    "devuan"			)
      DIST_ID="dvu${VERSION_ID}"
      ;;
    "fedora"			)
      DIST_ID="fc${VERSION_ID}"
      ;;
    "gentoo"			)
      DIST_ID="gentoo"
      ;;
    "opensuse-tumbleweed"	)
      DIST_ID="tw${VERSION_ID}"
      ;;
    "osukiss"	)
      DIST_ID="ok.${VERSION_ID}"
      ;;
    "ubuntu"			)
      DIST_ID="${VERSION_CODENAME}"
      ;;
    "void"			)
      DIST_ID="${ID}"
      ;;
    *				)
      DIST_ID="unset.999";
      ;;
    esac
  fi
echo ${DIST_ID}
