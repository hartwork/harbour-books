/*
 * Copyright (C) 2015 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BOOKS_LOADING_PROPERTY_H
#define BOOKS_LOADING_PROPERTY_H

#include "BooksTypes.h"

class BooksLoadingSignalBlocker;

class BooksLoadingProperty
{
    friend class BooksLoadingSignalBlocker;

public:
    BooksLoadingProperty() : iChangesBlocked(0), iSavedState(false) {}

    virtual bool loading() const = 0;
    virtual void loadingChanged() = 0;

private:
    void suspendLoadingStateChanges();
    void resumeLoadingStateChanges();

    int iChangesBlocked;
    bool iSavedState;
};

class BooksLoadingSignalBlocker {
public:
    BooksLoadingSignalBlocker(BooksLoadingProperty* aProperty) : iProperty(aProperty)
        { iProperty->suspendLoadingStateChanges(); }
    ~BooksLoadingSignalBlocker()
        { iProperty->resumeLoadingStateChanges(); }
private:
    BooksLoadingProperty* iProperty;
};

#endif // BOOKS_LOADING_PROPERTY_H
