#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QMessageBox>
#include <QVBoxLayout>

#include "mainwindow.hpp"
#include "welcomewindow.hpp"

WelcomeWindow::WelcomeWindow(client::Client& cl, QWidget* parent)
    : QWidget(parent), client{cl} {
  setupUI();
  setWindowTitle("Подключение к серверу");
  setFixedSize(400, 200);
}

WelcomeWindow::~WelcomeWindow() {}

void WelcomeWindow::setupUI() {
  QVBoxLayout* mainLayout = new QVBoxLayout(this);

  // Заголовок
  QLabel* titleLabel = new QLabel("Введите данные сервера");
  titleLabel->setAlignment(Qt::AlignCenter);
  titleLabel->setStyleSheet(
      "font-size: 16px; font-weight: bold; margin-bottom: 10px;");
  mainLayout->addWidget(titleLabel);

  // Форма ввода
  QFormLayout* formLayout = new QFormLayout();

  ipLineEdit = new QLineEdit();
  formLayout->addRow("IP адрес:", ipLineEdit);

  portLineEdit = new QLineEdit();
  portLineEdit->setValidator(new QIntValidator(1, 65535, this));
  formLayout->addRow("Порт:", portLineEdit);

  mainLayout->addLayout(formLayout);

  // Статус подключения
  statusLabel = new QLabel();
  statusLabel->setAlignment(Qt::AlignCenter);
  mainLayout->addWidget(statusLabel);

  // Кнопка подключения
  connectButton = new QPushButton("Подключиться");
  connectButton->setStyleSheet(
      "QPushButton {"
      "   background-color: #4CAF50;"
      "   color: white;"
      "   padding: 10px;"
      "   font-size: 14px;"
      "   border-radius: 5px;"
      "}"
      "QPushButton:hover {"
      "   background-color: #45a049;"
      "}");
  mainLayout->addWidget(connectButton);

  connect(connectButton, &QPushButton::clicked, this,
          &WelcomeWindow::onConnectClicked);
}

void WelcomeWindow::onConnectClicked() {
  QString ip = ipLineEdit->text().trimmed();
  QString portStr = portLineEdit->text().trimmed();

  statusLabel->setStyleSheet("color: gray; margin: 10px;");
  statusLabel->setText("Подключение...");

  connectButton->setEnabled(false);
  if (ip.isEmpty() || portStr.isEmpty()) {
    statusLabel->setStyleSheet("color: red; margin: 10px;");
    statusLabel->setText("Заполните все поля!");
    connectButton->setEnabled(true);
    return;
  }

  bool ok;
  int port = portStr.toInt(&ok);
  if (!ok || port < 1 || port > 65535) {
    statusLabel->setStyleSheet("color: red; margin: 10px;");
    statusLabel->setText("Некорректный порт!");
    connectButton->setEnabled(true);
    return;
  }

  QCoreApplication::processEvents();

  if (!client.connect(inet_addr(ip.toUtf8().constData()), htons(port))) {
    statusLabel->setStyleSheet("color: red; margin: 10px;");
    statusLabel->setText("Не удалось подключиться!");
    connectButton->setEnabled(true);
    return;
  }

  // Создаем и показываем основное окно
  MainWindow* mainWindow = new MainWindow(client);
  mainWindow->show();

  // Закрываем приветственное окно
  this->close();
}
