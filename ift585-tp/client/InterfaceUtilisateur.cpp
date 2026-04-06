// =============================================================
//  InterfaceUtilisateur.cpp  –  Interface graphique Qt5
//  IFT585 – TP4
// =============================================================
#include "InterfaceUtilisateur.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>

// ================================================================
//  LoginDialog
// ================================================================
LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("IFT585-TP – Connexion");
    setFixedSize(350, 220);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QGroupBox* grp = new QGroupBox("Paramètres de connexion", this);
    QVBoxLayout* grpLayout = new QVBoxLayout(grp);

    auto makeRow = [&](const QString& label, QLineEdit*& edit, bool password = false) {
        QHBoxLayout* row = new QHBoxLayout();
        QLabel* lbl = new QLabel(label);
        lbl->setFixedWidth(100);
        edit = new QLineEdit();
        if (password) edit->setEchoMode(QLineEdit::Password);
        row->addWidget(lbl);
        row->addWidget(edit);
        grpLayout->addLayout(row);
    };

    makeRow("Serveur IP :", ipEdit_);
    makeRow("Utilisateur :", userEdit_);
    makeRow("Mot de passe :", passEdit_, true);

    grp->setLayout(grpLayout);
    layout->addWidget(grp);

    statusLabel_ = new QLabel("", this);
    statusLabel_->setStyleSheet("color: red;");
    layout->addWidget(statusLabel_);

    connectBtn_ = new QPushButton("Connexion", this);
    connectBtn_->setDefault(true);
    layout->addWidget(connectBtn_);

    connect(connectBtn_, &QPushButton::clicked, this, &LoginDialog::onConnectClicked);

    ipEdit_->setText("127.0.0.1");
}

void LoginDialog::onConnectClicked() {
    if (ipEdit_->text().isEmpty() || userEdit_->text().isEmpty() || passEdit_->text().isEmpty()) {
        statusLabel_->setText("Veuillez remplir tous les champs.");
        return;
    }
    emit loginRequested(ipEdit_->text(), userEdit_->text(), passEdit_->text());
    accept();
}

std::string LoginDialog::getServerIp()  const { return ipEdit_->text().toStdString(); }
std::string LoginDialog::getUsername()  const { return userEdit_->text().toStdString(); }
std::string LoginDialog::getPassword()  const { return passEdit_->text().toStdString(); }

// ================================================================
//  MainWindow – Construction
// ================================================================
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    setupConnections();

    // Timer de polling 30 s
    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(30000);
    connect(pollTimer_, &QTimer::timeout, this, &MainWindow::onPollingTimer);
    pollTimer_->start();
}

void MainWindow::setupUi() {
    setWindowTitle("IFT585-TP – Système de partage de fichiers");
    setMinimumSize(900, 600);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // ---- Barre du haut ----
    QHBoxLayout* topBar = new QHBoxLayout();
    usernameLabel_ = new QLabel("Connecté : ", this);
    usernameLabel_->setStyleSheet("font-weight: bold; font-size: 13px;");
    topBar->addWidget(usernameLabel_);
    topBar->addStretch();
    syncStatusLabel_ = new QLabel("● Synchronisé", this);
    syncStatusLabel_->setStyleSheet("color: green; font-weight: bold;");
    topBar->addWidget(syncStatusLabel_);
    logoutBtn_ = new QPushButton("Déconnexion", this);
    topBar->addWidget(logoutBtn_);
    mainLayout->addLayout(topBar);

    // ---- Splitter principal ----
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    // ---- Panneau gauche : répertoires ----
    QGroupBox* dirGroup = new QGroupBox("Répertoires partagés", this);
    QVBoxLayout* dirLayout = new QVBoxLayout(dirGroup);
    dirList_ = new QListWidget(dirGroup);
    dirLayout->addWidget(dirList_);
    QHBoxLayout* dirBtns = new QHBoxLayout();
    createDirBtn_ = new QPushButton("+ Nouveau", this);
    refreshBtn_   = new QPushButton("↻ Rafraîchir", this);
    dirBtns->addWidget(createDirBtn_);
    dirBtns->addWidget(refreshBtn_);
    dirLayout->addLayout(dirBtns);
    dirGroup->setLayout(dirLayout);
    splitter->addWidget(dirGroup);

    // ---- Panneau central : membres ----
    QGroupBox* membGroup = new QGroupBox("Membres", this);
    QVBoxLayout* membLayout = new QVBoxLayout(membGroup);
    memberList_ = new QListWidget(membGroup);
    membLayout->addWidget(memberList_);
    QVBoxLayout* membBtns = new QVBoxLayout();
    inviteBtn_       = new QPushButton("Inviter", this);
    removeMemberBtn_ = new QPushButton("Retirer", this);
    transferAdminBtn_= new QPushButton("Nommer admin", this);
    membBtns->addWidget(inviteBtn_);
    membBtns->addWidget(removeMemberBtn_);
    membBtns->addWidget(transferAdminBtn_);
    membLayout->addLayout(membBtns);
    membGroup->setLayout(membLayout);
    splitter->addWidget(membGroup);

    // ---- Panneau droit : notifications + logs ----
    QTabWidget* tabs = new QTabWidget(this);

    // Onglet Invitations
    QWidget* invTab = new QWidget();
    QVBoxLayout* invLayout = new QVBoxLayout(invTab);
    invitationList_ = new QListWidget(invTab);
    invLayout->addWidget(invitationList_);
    acceptInvBtn_ = new QPushButton("Accepter", invTab);
    invLayout->addWidget(acceptInvBtn_);
    invTab->setLayout(invLayout);
    tabs->addTab(invTab, "Notifications");

    // Onglet Journal
    QWidget* logTab = new QWidget();
    QVBoxLayout* logLayout = new QVBoxLayout(logTab);
    transferLog_ = new QListWidget(logTab);
    logLayout->addWidget(transferLog_);
    logTab->setLayout(logLayout);
    tabs->addTab(logTab, "Journal");

    splitter->addWidget(tabs);
    splitter->setSizes({220, 200, 300});
    mainLayout->addWidget(splitter);

    // ---- Barre de statut ----
    statusBar()->showMessage("Prêt");
}

void MainWindow::setupConnections() {
    connect(createDirBtn_,    &QPushButton::clicked, this, &MainWindow::onCreateDirClicked);
    connect(inviteBtn_,       &QPushButton::clicked, this, &MainWindow::onInviteClicked);
    connect(removeMemberBtn_, &QPushButton::clicked, this, &MainWindow::onRemoveMemberClicked);
    connect(transferAdminBtn_,&QPushButton::clicked, this, &MainWindow::onTransferAdminClicked);
    connect(acceptInvBtn_,    &QPushButton::clicked, this, &MainWindow::onAcceptInvClicked);
    connect(refreshBtn_,      &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(logoutBtn_,       &QPushButton::clicked, this, &MainWindow::onLogoutClicked);
    connect(dirList_, &QListWidget::currentRowChanged, this, &MainWindow::onDirSelectionChanged);
}

// ================================================================
//  Mise à jour des données
// ================================================================
void MainWindow::setUsername(const std::string& user) {
    usernameLabel_->setText(QString("Connecté : %1").arg(QString::fromStdString(user)));
}

void MainWindow::setDirectories(const QStringList& dirs, const QStringList& ids) {
    dirIds_ = ids;
    dirList_->clear();
    dirList_->addItems(dirs);
}

void MainWindow::setMembers(const QStringList& members) {
    memberList_->clear();
    memberList_->addItems(members);
}

void MainWindow::setInvitations(const QStringList& invitations, const QStringList& ids) {
    invIds_ = ids;
    invitationList_->clear();
    invitationList_->addItems(invitations);
}

void MainWindow::setSyncStatus(const std::string& status) {
    if (status == "syncing") {
        syncStatusLabel_->setText("⟳ Synchronisation en cours...");
        syncStatusLabel_->setStyleSheet("color: orange; font-weight: bold;");
    } else if (status == "offline") {
        syncStatusLabel_->setText("✕ Hors ligne");
        syncStatusLabel_->setStyleSheet("color: red; font-weight: bold;");
    } else {
        syncStatusLabel_->setText("● Synchronisé");
        syncStatusLabel_->setStyleSheet("color: green; font-weight: bold;");
    }
}

void MainWindow::appendTransferLog(const std::string& msg) {
    QString ts = QDateTime::currentDateTime().toString("[hh:mm:ss] ");
    transferLog_->addItem(ts + QString::fromStdString(msg));
    transferLog_->scrollToBottom();
}

// ================================================================
//  Slots
// ================================================================
void MainWindow::onCreateDirClicked() {
    bool ok;
    QString name = QInputDialog::getText(this, "Nouveau répertoire",
                                          "Nom du répertoire :", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty() && onCreateDir_)
        onCreateDir_(name.toStdString());
}

void MainWindow::onInviteClicked() {
    if (selectedDirId_.isEmpty()) {
        QMessageBox::warning(this, "Invitation", "Sélectionnez d'abord un répertoire.");
        return;
    }
    bool ok;
    QString user = QInputDialog::getText(this, "Inviter un utilisateur",
                                          "Nom d'utilisateur :", QLineEdit::Normal, "", &ok);
    if (ok && !user.isEmpty() && onInviteUser_)
        onInviteUser_(selectedDirId_.toStdString(), user.toStdString());
}

void MainWindow::onRemoveMemberClicked() {
    if (selectedDirId_.isEmpty()) return;
    QListWidgetItem* item = memberList_->currentItem();
    if (!item) { QMessageBox::warning(this, "Retirer", "Sélectionnez un membre."); return; }
    QString user = item->text();
    if (QMessageBox::question(this, "Retirer membre",
            QString("Retirer %1 du répertoire ?").arg(user)) == QMessageBox::Yes) {
        if (onRemoveMember_)
            onRemoveMember_(selectedDirId_.toStdString(), user.toStdString());
    }
}

void MainWindow::onTransferAdminClicked() {
    if (selectedDirId_.isEmpty()) return;
    QListWidgetItem* item = memberList_->currentItem();
    if (!item) { QMessageBox::warning(this, "Admin", "Sélectionnez un membre."); return; }
    QString user = item->text();
    if (QMessageBox::question(this, "Nommer administrateur",
            QString("Transférer les droits admin à %1 ?").arg(user)) == QMessageBox::Yes) {
        if (onTransferAdmin_)
            onTransferAdmin_(selectedDirId_.toStdString(), user.toStdString());
    }
}

void MainWindow::onAcceptInvClicked() {
    int row = invitationList_->currentRow();
    if (row < 0 || row >= invIds_.size()) {
        QMessageBox::warning(this, "Invitation", "Sélectionnez une invitation."); return;
    }
    if (onAcceptInv_)
        onAcceptInv_(invIds_[row].toStdString());
}

void MainWindow::onRefreshClicked() {
    if (onRefreshDirs_) onRefreshDirs_();
}

void MainWindow::onLogoutClicked() {
    if (QMessageBox::question(this, "Déconnexion", "Se déconnecter ?") == QMessageBox::Yes) {
        if (onLogout_) onLogout_();
    }
}

void MainWindow::onDirSelectionChanged() {
    int row = dirList_->currentRow();
    if (row >= 0 && row < dirIds_.size()) {
        selectedDirId_ = dirIds_[row];
        if (onRefreshDirs_) onRefreshDirs_();
    }
}

void MainWindow::onPollingTimer() {
    if (onRefreshDirs_) onRefreshDirs_();
}
