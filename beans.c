/* beans is a simple pastebin server */

#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT   "2023"
#define BACKLOG 32 /* XXX What's a reasonable value? */
#define BASEURI "https://paste.bitmanent.org/p"

void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

int
bindon(char *port) {
	struct addrinfo hints, *res;
	int sd = 0, e;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if((e = getaddrinfo(NULL, port, &hints, &res)))
		die("getaddrinfo(): %s\n", gai_strerror(e));
	if((sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
		die("socket(): %s\n", strerror(errno));
	if(bind(sd, res->ai_addr, res->ai_addrlen) == -1)
		die("bind(): %s\n", strerror(errno));
	if(listen(sd, BACKLOG) == -1)
		die("listen(): %s\n", strerror(errno));
	return sd;
}

void *
ecalloc(size_t nmemb, size_t size) {
	void *p;

	if(!(p = calloc(nmemb, size)))
		die("Cannot allocate memory.\n");
	return p;
}

char *
readall(int sd, int *len) {
	char *buf;
	int sz = 512, r, l = 0;

	buf = ecalloc(1, sz);
	while((r = read(sd, &buf[l], sz - l)) != -1) {
		if(!r)
			break;
		l += r;
		if(l == sz) {
			sz *= 2;
			if(!(buf = realloc(buf, sz)))
				die("realloc()\n");
		}
	}
	if(r == -1) {
		free(buf);
		return NULL;
	}
	if(len)
		*len = l;
	buf[l] = '\0';
	return buf;
}

void
sout(int sd, char *fmt, ...) {
	va_list ap;
	char buf[4096];
	int sz;

	va_start(ap, fmt);
	sz = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	send(sd, buf, sz, 0);
}

int
main(int argc, char *argv[]) {
	struct sockaddr_storage conn;
	socklen_t size;
	int sd, csd, len, tmpsd;
	char tmpfn[64] = {0}, *buf, *p;

	sd = bindon(PORT);
	size = sizeof conn;
	while(1) {
		csd = accept(sd, (struct sockaddr *)&conn, &size);
		if(csd == -1) {
			fprintf(stderr, "accept(): %s", strerror(errno));
			continue;
		}
		buf = readall(csd, &len);
		if(!(buf && len)) {
			sout(csd, "Nothing pasted.\n");
			close(csd);
			continue;
		}
		snprintf(tmpfn, sizeof tmpfn, "/tmp/beans.XXXXXX");
		tmpsd = mkstemp(tmpfn);
		if(tmpsd == -1) {
			fprintf(stderr, "mkstemp()\n");
			free(buf);
			close(csd);
			continue;
		}
		if(write(tmpsd, buf, len) == -1)
			fprintf(stderr, "write(): %s\n", strerror(errno));
		p = strchr(tmpfn, '.');
		if(p && ++p)
			sout(csd, "%s/%s\n", BASEURI, p);
		else
			sout(csd, "Paste error.\n");
		close(csd);
		close(tmpsd);
		free(buf);
	}
	close(sd);
	return 0;
}