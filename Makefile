#
# Copyright (C) 2015-2017 Colin Ian King
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Author: Colin Ian King <colin.i.king@gmail.com>
#

VERSION=0.01.10

CFLAGS += -Wall -Wextra -DVERSION='"$(VERSION)"' -O2
LDFLAGS += -lncurses


# Pedantic flags
#
ifeq ($(PEDANTIC),1)
CFLAGS += -Wabi -Wcast-qual -Wfloat-equal -Wmissing-declarations \
	-Wmissing-format-attribute -Wno-long-long -Wpacked \
	-Wredundant-decls -Wshadow -Wno-missing-field-initializers \
	-Wno-missing-braces -Wno-sign-compare -Wno-multichar
endif

BINDIR=/usr/sbin
MANDIR=/usr/share/man/man8

SRC = pagemon.c perf.c
OBJS = $(SRC:.c=.o)

pagemon: $(OBJS) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

pagemon.o: pagemon.c perf.h Makefile
perf.o: perf.c perf.h Makefile

pagemon.8.gz: pagemon.8
	gzip -c $< > $@

dist:
	rm -rf pagemon-$(VERSION)
	mkdir pagemon-$(VERSION)
	cp -rp README Makefile pagemon.c pagemon.8 perf.c perf.h COPYING pagemon-$(VERSION)
	tar -zcf pagemon-$(VERSION).tar.gz pagemon-$(VERSION)
	rm -rf pagemon-$(VERSION)

clean:
	rm -f pagemon pagemon.o pagemon.8.gz pagemon-$(VERSION).tar.gz

install: pagemon pagemon.8.gz
	mkdir -p ${DESTDIR}${BINDIR}
	cp pagemon ${DESTDIR}${BINDIR}
	mkdir -p ${DESTDIR}${MANDIR}
	cp pagemon.8.gz ${DESTDIR}${MANDIR}
