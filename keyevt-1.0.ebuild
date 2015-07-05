# Ebuild written by Alexander Holler
# Distributed under the terms of the GNU General Public License v2

EAPI="5"

KEYWORDS="arm amd64 x86"

DESCRIPTION="A simple key event daemon"
HOMEPAGE="https://github.com/aholler/keyevt"

LICENSE="GPL-2"
SLOT="0"

src_unpack() {
	mkdir "$S"
	cp "${FILESDIR}"/keyevt.cpp "$S" || die
}

src_compile() {
	local t="g++ $CXXFLAGS $LDFLAGS -Wall -pedantic -std=gnu++11 -pthread -o keyevt keyevt.cpp"
	echo $t
	$t
}

src_install() {
	dobin keyevt
	cat > keyevt.init << END
#!/sbin/runscript

start() {
	ebegin "Starting keyevt"
	start-stop-daemon --start --background --quiet \\
		--pidfile /run/keyevt.pid --make-pidfile \\
		--exec /usr/bin/keyevt -- "\${INPUTDEV}" "\${CONFIG}"
	eend \$? "Failed to start keyevt"
}

stop() {
	ebegin "Stopping keyevt"
	start-stop-daemon --stop --quiet \\
		--pidfile /run/keyevt.pid \\
		--exec /usr/bin/keyevt
	eend \$? "Failed to stop keyevt"
}
END
	newinitd keyevt.init keyevt
	cat > keyevt.conf << END
INPUTDEV=/dev/input/event0
CONFIG=/etc/keyevt.cfg
END
	newconfd keyevt.conf keyevt
	insinto /etc
	doins "$FILESDIR"/keyevt.cfg
}
