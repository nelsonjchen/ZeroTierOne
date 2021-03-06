/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>

#include "node/Identity.hpp"
#include "node/Utils.hpp"

using namespace ZeroTier;

static void printHelp(char *pn)
{
	std::cout << "Usage: " << pn << " <command> [<args>]" << std::endl << std::endl;
	std::cout << "Commands:" << std::endl;
	std::cout << "\tgenerate [<identity.secret>]" << std::endl;
	std::cout << "\tvalidate <identity.secret/public>" << std::endl;
	std::cout << "\tgetpublic <identity.secret>" << std::endl;
	std::cout << "\tsign <identity.secret> <file>" << std::endl;
	std::cout << "\tverify <identity.secret/public> <file> <signature>" << std::endl;
	std::cout << "\tencrypt <identity.secret> <identity.public (recipient)> [<file>] [<outfile>]" << std::endl;
	std::cout << "\tdecrypt <identity.secret> <identity.public (sender)> [<file>] [<outfile>]" << std::endl;
}

static Identity getIdFromArg(char *arg)
{
	Identity id;
	if ((strlen(arg) > 32)&&(arg[10] == ':')) { // identity is a literal on the command line
		if (id.fromString(arg))
			return id;
	} else { // identity is to be read from a file
		std::string idser;
		if (Utils::readFile(arg,idser)) {
			if (id.fromString(idser))
				return id;
		}
	}
	return Identity();
}

int main(int argc,char **argv)
{
	if (argc < 2) {
		printHelp(argv[0]);
		return -1;
	}

	if (!strcmp(argv[1],"generate")) {
		Identity id;
		id.generate();
		std::string idser = id.toString(true);
		if (argc >= 3) {
			if (!Utils::writeFile(argv[2],idser)) {
				std::cerr << "Error writing to " << argv[2] << std::endl;
				return -1;
			} else std::cout << argv[2] << " written" << std::endl;
		} else std::cout << idser;
	} else if (!strcmp(argv[1],"validate")) {
		if (argc < 3) {
			printHelp(argv[0]);
			return -1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return -1;
		}

		if (!id.locallyValidate(true)) {
			std::cerr << argv[2] << " FAILED validation." << std::endl;
			return -1;
		} else std::cout << argv[2] << " is a valid identity (full check performed)" << std::endl;
	} else if (!strcmp(argv[1],"getpublic")) {
		if (argc < 3) {
			printHelp(argv[0]);
			return -1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return -1;
		}

		std::cout << id.toString(false);
	} else if (!strcmp(argv[1],"sign")) {
		if (argc < 4) {
			printHelp(argv[0]);
			return -1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return -1;
		}

		if (!id.hasPrivate()) {
			std::cerr << argv[2] << " does not contain a private key (must use private to sign)" << std::endl;
			return -1;
		}

		std::string inf;
		if (!Utils::readFile(argv[3],inf)) {
			std::cerr << argv[3] << " is not readable" << std::endl;
			return -1;
		}
		std::string signature = id.sign(inf.data(),inf.length());
		if (!signature.length()) {
			std::cerr << "Error computing SHA256 or signature (bug?)" << std::endl;
			return -1;
		}

		std::cout << Utils::base64Encode(signature.data(),signature.length());
	} else if (!strcmp(argv[1],"verify")) {
		if (argc < 4) {
			printHelp(argv[0]);
			return -1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return -1;
		}

		std::string inf;
		if (!Utils::readFile(argv[3],inf)) {
			std::cerr << argv[3] << " is not readable" << std::endl;
			return -1;
		}

		std::string signature(Utils::base64Decode(argv[4],strlen(argv[4])));
		if ((signature.length() > ZEROTIER_ADDRESS_LENGTH)&&(id.verifySignature(inf.data(),inf.length(),signature))) {
			std::cout << argv[3] << " signature valid" << std::endl;
		} else {
			std::cerr << argv[3] << " signature check FAILED" << std::endl;
			return -1;
		}
	} else if (!strcmp(argv[1],"encrypt")) {
		if (argc < 4) {
			printHelp(argv[0]);
			return -1;
		}

		Identity from = getIdFromArg(argv[2]);
		if (!from) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return -1;
		}
		if (!from.hasPrivate()) {
			std::cerr << argv[2] << " must contain a secret key" << std::endl;
			return -1;
		}

		Identity to = getIdFromArg(argv[3]);
		if (!to) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[3] << std::endl;
			return -1;
		}

		FILE *inf;
		if (argc > 4) {
			inf = fopen(argv[4],"r");
			if (!inf) {
				std::cerr << "Could not open input file " << argv[4] << std::endl;
				return -1;
			}
		} else inf = stdin;
		int inbuflen = 131072;
		char *inbuf = (char *)malloc(inbuflen);
		if (!inbuf) {
			std::cerr << "Could not malloc()" << std::endl;
			return -1;
		}
		int inlen = 0;
		int n;
		while ((n = (int)fread(inbuf + inlen,1,inbuflen - inlen,inf)) > 0) {
			inlen += n;
			if ((inbuflen - inlen) < 1024) {
				inbuf = (char *)realloc(inbuf,inbuflen += 131072);
				if (!inbuf) {
					std::cerr << "Could not malloc()" << std::endl;
					return -1;
				}
			}
		}
		if (inf != stdin)
			fclose(inf);

		std::string crypted(from.encrypt(to,inbuf,inlen));
		if (!crypted.length()) {
			std::cerr << "Failure encrypting data, check from/to identities" << std::endl;
			return -1;
		}

		if (argc > 5)
			Utils::writeFile(argv[5],crypted.data(),crypted.length());
		else fwrite(crypted.data(),1,crypted.length(),stdout);

		free(inbuf);
	} else if (!strcmp(argv[1],"decrypt")) {
		if (argc < 4) {
			printHelp(argv[0]);
			return -1;
		}

		Identity to = getIdFromArg(argv[2]);
		if (!to) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return -1;
		}

		if (!to.hasPrivate()) {
			std::cerr << argv[2] << " must contain a secret key" << std::endl;
			return -1;
		}

		Identity from = getIdFromArg(argv[3]);
		if (!from) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[3] << std::endl;
			return -1;
		}

		FILE *inf;
		if (argc > 4) {
			inf = fopen(argv[4],"r");
			if (!inf) {
				std::cerr << "Could not open input file " << argv[4] << std::endl;
				return -1;
			}
		} else inf = stdin;
		int inbuflen = 131072;
		char *inbuf = (char *)malloc(inbuflen);
		if (!inbuf) {
			std::cerr << "Could not malloc()" << std::endl;
			return -1;
		}
		int inlen = 0;
		int n;
		while ((n = (int)fread(inbuf + inlen,1,inbuflen - inlen,inf)) > 0) {
			inlen += n;
			if ((inbuflen - inlen) < 1024) {
				inbuf = (char *)realloc(inbuf,inbuflen += 131072);
				if (!inbuf) {
					std::cerr << "Could not malloc()" << std::endl;
					return -1;
				}
			}
		}
		if (inf != stdin)
			fclose(inf);

		std::string dec(to.decrypt(from,inbuf,inlen));
		free(inbuf);
		if (!dec.length()) {
			std::cerr << "Failure decrypting data, check from/to identities" << std::endl;
			return -1;
		}

		if (argc > 5)
			Utils::writeFile(argv[5],dec.data(),dec.length());
		else fwrite(dec.data(),1,dec.length(),stdout);
	} else {
		printHelp(argv[0]);
		return -1;
	}

	return 0;
}
