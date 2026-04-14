#pragma once
// =============================================================
//  InterfaceUtilisateur.h  –  Interface graphique Qt5
//  IFT585 – TP4
//
//  Composants :
//    - Fenêtre de connexion
//    - Tableau de bord principal (répertoires, membres, transferts)
//    - Panneau notifications (invitations)
// =============================================================
#include <QMainWindow>
#include <QWidget>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QTabWidget>
#include <string>
#include <functional>

// ================================================================
//  Fenêtre de connexion
// ================================================================
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);

    std::string getServerIp()  const;
    std::string getUsername()  const;
    std::string getPassword()  const;

signals:
    void loginRequested(const QString& ip, const QString& user, const QString& pass);

private slots:
    void onConnectClicked();

private:
    QLineEdit* ipEdit_;
    QLineEdit* userEdit_;
    QLineEdit* passEdit_;
    QPushButton* connectBtn_;
    QLabel*    statusLabel_;
};

// ================================================================
//  Fenêtre principale (tableau de bord)
// ================================================================
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Peuplement des listes
    void setUsername(const std::string& user);
    void setDirectories(const QStringList& dirs, const QStringList& ids);
    void setMembers(const QStringList& members);
    void setInvitations(const QStringList& invitations, const QStringList& ids);
    void setFiles(const QList<QStringList>& rows);
    void setLocalPath(const std::string& path);
    void setSyncStatus(const std::string& status);
    void appendTransferLog(const std::string& msg);
    QString getSelectedDirId() const { return selectedDirId_; }

    // Callbacks vers ClientApp
    using Callback0        = std::function<void()>;
    using CallbackStr      = std::function<void(const std::string&)>;
    using Callback2Str     = std::function<void(const std::string&, const std::string&)>;

    void setOnCreateDir(CallbackStr cb)        { onCreateDir_       = cb; }
    void setOnInviteUser(Callback2Str cb)      { onInviteUser_      = cb; }
    void setOnRemoveMember(Callback2Str cb)    { onRemoveMember_    = cb; }
    void setOnTransferAdmin(Callback2Str cb)   { onTransferAdmin_   = cb; }
    void setOnAcceptInvitation(CallbackStr cb)  { onAcceptInv_       = cb; }
    void setOnDeclineInvitation(CallbackStr cb) { onDeclineInv_     = cb; }
    void setOnRefreshDirs(Callback0 cb)         { onRefreshDirs_    = cb; }
    void setOnLogout(Callback0 cb)              { onLogout_         = cb; }
    void setOnlineUsers(const QStringList& users) { onlineUsers_    = users; }

signals:
    void syncStatusChanged(const QString& status);

private slots:
    void onCreateDirClicked();
    void onInviteClicked();
    void onRemoveMemberClicked();
    void onTransferAdminClicked();
    void onAcceptInvClicked();
    void onDeclineInvClicked();
    void onRefreshClicked();
    void onLogoutClicked();
    void onDirSelectionChanged();
    void onPollingTimer();

private:
    // Widgets
    QLabel*      usernameLabel_;
    QListWidget* dirList_;
    QListWidget* memberList_;
    QListWidget* invitationList_;
    QListWidget* transferLog_;
    QTreeWidget* fileList_;
    QLabel*      localPathLabel_;
    QLabel*      syncStatusLabel_;
    QPushButton* createDirBtn_;
    QPushButton* inviteBtn_;
    QPushButton* removeMemberBtn_;
    QPushButton* transferAdminBtn_;
    QPushButton* acceptInvBtn_;
    QPushButton* declineInvBtn_;
    QPushButton* refreshBtn_;
    QPushButton* logoutBtn_;

    // Données
    QStringList dirIds_;
    QStringList invIds_;
    QString     selectedDirId_;
    QStringList onlineUsers_;

    // Timer de polling (30 s)
    QTimer* pollTimer_;

    // Callbacks
    CallbackStr   onCreateDir_;
    Callback2Str  onInviteUser_;
    Callback2Str  onRemoveMember_;
    Callback2Str  onTransferAdmin_;
    CallbackStr   onAcceptInv_;
    CallbackStr   onDeclineInv_;
    Callback0     onRefreshDirs_;
    Callback0     onLogout_;

    void setupUi();
    void setupConnections();
};
