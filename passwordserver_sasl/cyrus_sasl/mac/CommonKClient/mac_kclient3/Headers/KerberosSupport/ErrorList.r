/* $Copyright: * * Copyright 1998-2000 by the Massachusetts Institute of Technology. *  * All rights reserved. *  * Permission to use, copy, modify, and distribute this software and its * documentation for any purpose and without fee is hereby granted, * provided that the above copyright notice appear in all copies and that * both that copyright notice and this permission notice appear in * supporting documentation, and that the name of M.I.T. not be used in * advertising or publicity pertaining to distribution of the software * without specific, written prior permission.  Furthermore if you modify * this software you must label your software as modified software and not * distribute it in such a fashion that it might be confused with the * original MIT software. M.I.T. makes no representations about the * suitability of this software for any purpose.  It is provided "as is" * without express or implied warranty. *  * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. *  * Individual source code files are copyright MIT, Cygnus Support, * OpenVision, Oracle, Sun Soft, FundsXpress, and others. *  * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira, * and Zephyr are trademarks of the Massachusetts Institute of Technology * (MIT).  No commercial use of these trademarks may be made without prior * written permission of MIT. *  * "Commercial use" means use of a name in a product or other for-profit * manner.  It does NOT prevent a commercial firm from referring to the MIT * trademarks in order to convey information (although in doing so, * recognition of their trademark status should be given). * $ *//* $Header: /cvs/repository/iservers/Libraries/passwordserver_sasl/cyrus_sasl/mac/CommonKClient/mac_kclient3/Headers/KerberosSupport/ErrorList.r,v 1.1 2002/02/28 00:28:04 snsimon Exp $ */type 'ErrT' {	integer = $$CountOf (ErrorTable);		//	Number of errors in the table	align long;	wide array ErrorTable {EntryStart:											//	Calculate the length of this											//	array element (in bytes)		integer = (EntryEnd [$$ArrayIndex (ErrorTable)] -					EntryStart [$$ArrayIndex (ErrorTable)]) / 8;		align long;		longint;							//	ErrorCode		cstring;							//	Short error string		align long;		cstring;							//	Long error string		align long;EntryEnd:	};};/* sampleformat: error number, short error string, long error stringerror numbers don't have to be consecutiveresource 'ErrT' (129, "Manager Name"){	{		-1, "Short 1", "Long 1",		-2, "Short 2", "Long 2"	}};*/