#!/usr/bin/perl 

print <<_EOF;
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/* 
 * Copyright 2007-2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * CDDL HEADER START
 * 
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 * 
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * 
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 * 
 * CDDL HEADER END
 * 
 * File:   help_refs.h
 *
 */

#ifndef _HELP_REFS_H
#define	_HELP_REFS_H

G_BEGIN_DECLS

_EOF

while( <> ) {
    # if ( m/<\(chapter\|sect[123]\)\s+id="\([^"]+\)\s+<title>\(.*\)<.title>.*">/ ) {
    # if ( m/<(chapter|sect[123])\s+id="([^"]+)\s+<title>(.*)<.title>.*">/ ) {
    if ( m/<(chapter|sect[123])\s+id="([^"]+).*<title>(.*)<.title/ ) {
        $var = uc("HELP_REF_$2");
        $refname = $2;
        $title = $3;
        $var =~ s/-/_/g;

        printf("#define %-40s %-30s  /* %s */\n", $var, "\"$refname\"", $title );
        next;
    }
    else {
        next;
    }
}

print <<_EOF;

G_END_DECLS

#endif	/* _HELP_REFS_H */

_EOF
