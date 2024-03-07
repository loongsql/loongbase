/* Copyright (c) 2024 LoongSQL, Inc.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// SectionPage.cpp: implementation of the SectionPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SectionPage.h"
#include "RecordLocatorPage.h"
#include "Dbb.h"
#include "BDB.h"
#include "Validation.h"
#include "Section.h"
#include "IndexPage.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void SectionPage::validate(Dbb *dbb, Validation *validation, int sectionId,
                           int sequence, Bitmap *dataPages) {
  for (int n = 0; n < dbb->pagesPerSection; ++n) {
    int32 indexPageNumber = pages[n];

    if (indexPageNumber) {
      Bdb *indexBdb = dbb->fetchPage(indexPageNumber, PAGE_any, Shared);
      BDB_HISTORY(indexBdb);

      if (level == 0) {
        if (dbb->sequenceSectionId && (sectionId == dbb->sequenceSectionId)) {
          if (validation->isPageType(indexBdb, PAGE_sequences,
                                     "Sequences Page, section %d, sequence %d",
                                     sectionId, sequence + n)) {
            validation->inUse(indexPageNumber, "RecordLocatorPage");
          }
        } else if (validation->isPageType(
                       indexBdb, PAGE_record_locator,
                       "RecordLocatorPage, section %d, sequence %d", sectionId,
                       sequence + n)) {
          validation->inUse(indexPageNumber, "RecordLocatorPage");
          RecordLocatorPage *indexPage = (RecordLocatorPage *)indexBdb->buffer;

          if (!indexPage->validate(dbb, validation, sectionId,
                                   sequence * dbb->pagesPerSection + n,
                                   dataPages) &&
              validation->isRepair()) {
            indexBdb->release(REL_HISTORY);
            indexBdb =
                dbb->fetchPage(indexPageNumber, PAGE_record_locator, Exclusive);
            BDB_HISTORY(indexBdb);
            indexBdb->mark(0);
            indexPage = (RecordLocatorPage *)indexBdb->buffer;
            indexPage->repair(dbb, sectionId,
                              sequence * dbb->pagesPerSection + n);
          }
        }
      } else if (validation->isPageType(indexBdb, PAGE_sections,
                                        "SectionPage, section %d, sequence %d",
                                        sectionId, sequence + n)) {
        validation->inUse(indexPageNumber, "SectionPage");
        SectionPage *sectionPage = (SectionPage *)indexBdb->buffer;
        sectionPage->validate(dbb, validation, sectionId,
                              sequence * dbb->pagesPerSection + n, dataPages);
      }

      indexBdb->release(REL_HISTORY);
    }
  }
}

void SectionPage::validateSections(Dbb *dbb, Validation *validation, int base) {
  int pageNumber;

  if (level == 0) {
    for (int n = 0; n < dbb->pagesPerSection; ++n)
      if ((pageNumber = pages[n]))
        Section::validate(dbb, validation, base * dbb->pagesPerSection + n,
                          pageNumber);

    return;
  }

  for (int n = 0; n < dbb->pagesPerSection; ++n)
    if ((pageNumber = pages[n])) {
      int sequence = base * dbb->pagesPerSection + n;
      Bdb *bdb = dbb->fetchPage(pageNumber, PAGE_any, Shared);
      BDB_HISTORY(bdb);

      if (validation->isPageType(bdb, PAGE_sections,
                                 "SectionsPage, sequence %d", sequence)) {
        validation->inUse(pageNumber, "sections page");
        SectionPage *sectionPage = (SectionPage *)bdb->buffer;
        sectionPage->validateSections(dbb, validation, sequence);
      }

      bdb->release(REL_HISTORY);
    }
}

void SectionPage::validateIndexes(Dbb *dbb, Validation *validation, int base) {
  int pageNumber;

  if (level == 0) {
    for (int n = 0; n < dbb->pagesPerSection; ++n)
      if ((pageNumber = pages[n])) {
        Bdb *bdb = dbb->fetchPage(pageNumber, PAGE_any, Shared);
        BDB_HISTORY(bdb);

        if (validation->isPageType(bdb, PAGE_btree, "IndexPage, indexId %d",
                                   sequence)) {
          IndexPage *indexPage = (IndexPage *)bdb->buffer;
          validation->inUse(pageNumber, "IndexPage");
          Bitmap children;
          validation->indexId = base + n;
          indexPage->validate(dbb, validation, &children, bdb->pageNumber);
        }

        bdb->release(REL_HISTORY);
      }
    return;
  }

  for (int n = 0; n < dbb->pagesPerSection; ++n)
    if ((pageNumber = pages[n])) {
      int sequence = base * dbb->pagesPerSection + n;
      Bdb *bdb = dbb->fetchPage(pageNumber, PAGE_any, Shared);
      BDB_HISTORY(bdb);

      if (validation->isPageType(bdb, PAGE_sections,
                                 "Index SectionsPage, sequence %d", sequence)) {
        validation->inUse(pageNumber, "index sections page");
        SectionPage *sectionPage = (SectionPage *)bdb->buffer;
        sectionPage->validateIndexes(dbb, validation, sequence);
      } else
        validation->inUse(pageNumber, "corrupt index sections page");

      bdb->release(REL_HISTORY);
    }
}

void SectionPage::analyze(Dbb *dbb, SectionAnalysis *analysis, int sectionId,
                          int sequence, Bitmap *dataPages) {
  for (int n = 0; n < dbb->pagesPerSection; ++n) {
    int32 indexPageNumber = pages[n];

    if (indexPageNumber) {
      Bdb *indexBdb = dbb->fetchPage(indexPageNumber, PAGE_any, Shared);
      BDB_HISTORY(indexBdb);

      if (level == 0) {
        ++analysis->recordLocatorPages;
        RecordLocatorPage *indexPage = (RecordLocatorPage *)indexBdb->buffer;
        indexPage->analyze(dbb, analysis, sectionId,
                           sequence * dbb->pagesPerSection + n, dataPages);
      } else {
        SectionPage *sectionPage = (SectionPage *)indexBdb->buffer;
        sectionPage->analyze(dbb, analysis, sectionId,
                             sequence * dbb->pagesPerSection + n, dataPages);
      }

      indexBdb->release(REL_HISTORY);
    }
  }
}

void SectionPage::backup(EncodedDataStream *stream) {}

void SectionPage::restore(EncodedDataStream *stream) {}

}  // namespace Changjiang
