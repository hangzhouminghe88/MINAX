/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2020 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/

import QtQuick 2.10
import QtQuick.Window 2.10
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.1
import QtQuick.Dialogs 1.2

Page {
   id: tutorialPage
   visible: true
   height: parent ? parent.height : 0
   width: parent ? parent.width : 0

   SwipeView {
      id: tutorialSwipe
      height: parent.height
      width: parent.width

      FeaturesTutorialPage {}
      ConfigTutorialPage {}
   }

   PageIndicator {
       id: indicator
       count: tutorialSwipe.count
       currentIndex: tutorialSwipe.currentIndex
       anchors.bottom: tutorialSwipe.bottom
       anchors.horizontalCenter: parent.horizontalCenter
   }
}
