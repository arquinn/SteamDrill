#!/bin/bash

sudo logrotate -f /etc/logrotate.d/rsyslog || {
	echo "Log rotate failed? again!"
	exit 1
}

sudo kill -1 `cat /var/run/rsyslogd.pid`

