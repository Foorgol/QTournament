#-------------------------------------------------
#
# Project created by QtCreator 2014-09-12T13:04:40
#
#-------------------------------------------------

QT       += sql widgets

TARGET = QTournament
TEMPLATE = app
QMAKE_CXXFLAGS += -std=c++11

HEADERS += \
    Category.h \
    CatMngr.h \
    GenericDatabaseObject.h \
    GenericObjectManager.h \
    GroupDef.h \
    KO_Config.h \
    MatchGroup.h \
    MatchMngr.h \
    Player.h \
    PlayerMngr.h \
    PlayerPair.h \
    RoundRobinCategory.h \
    Team.h \
    TeamMngr.h \
    Tournament.h \
    TournamentDataDefs.h \
    TournamentDB.h \
    TournamentErrorCodes.h \
    ui/CatTableView.h \
    ui/CatTabWidget.h \
    ui/dlgEditPlayer.h \
    ui/dlgGroupAssignment.h \
    ui/GroupAssignmentListWidget.h \
    ui/GroupConfigWidget.h \
    ui/MainFrame.h \
    ui/PlayerTableView.h \
    ui/PlayerTabWidget.h \
    ui/TeamListView.h \
    ui/TeamTabWidget.h \
    ui/delegates/PairItemDelegate.h \
    ui/delegates/PlayerItemDelegate.h \
    models/CatTableModel.h \
    models/PlayerTableModel.h \
    models/TeamListModel.h

SOURCES += \
    Category.cpp \
    CatMngr.cpp \
    GenericDatabaseObject.cpp \
    GenericObjectManager.cpp \
    GroupDef.cpp \
    KO_Config.cpp \
    main.cpp \
    MatchGroup.cpp \
    MatchMngr.cpp \
    Player.cpp \
    PlayerMngr.cpp \
    PlayerPair.cpp \
    RoundRobinCategory.cpp \
    Team.cpp \
    TeamMngr.cpp \
    Tournament.cpp \
    TournamentDB.cpp \
    ui/CatTableView.cpp \
    ui/CatTabWidget.cpp \
    ui/dlgEditPlayer.cpp \
    ui/dlgGroupAssignment.cpp \
    ui/GroupAssignmentListWidget.cpp \
    ui/GroupConfigWidget.cpp \
    ui/MainFrame.cpp \
    ui/PlayerTableView.cpp \
    ui/PlayerTabWidget.cpp \
    ui/TeamListView.cpp \
    ui/TeamTabWidget.cpp \
    ui/delegates/PairItemDelegate.cpp \
    ui/delegates/PlayerItemDelegate.cpp \
    models/CatTableModel.cpp \
    models/PlayerTableModel.cpp \
    models/TeamListModel.cpp

RESOURCES += \
    tournament.qrc

OTHER_FILES +=

FORMS += \
    ui/CatTabWidget.ui \
    ui/dlgEditPlayer.ui \
    ui/dlgGroupAssignment.ui \
    ui/GroupAssignmentListWidget.ui \
    ui/GroupConfigWidget.ui \
    ui/MainFrame.ui \
    ui/PlayerTabWidget.ui \
    ui/TeamTabWidget.ui


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../DatabaseOverlayLib/release/ -lDatabaseOverlayLib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../DatabaseOverlayLib/debug/ -lDatabaseOverlayLib
else:unix:!macx: LIBS += -L$$PWD/../DatabaseOverlayLib/ -lDatabaseOverlayLib

INCLUDEPATH += $$PWD/../DatabaseOverlayLib
DEPENDPATH += $$PWD/../DatabaseOverlayLib
