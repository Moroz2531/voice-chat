#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

#include "client/client.hpp"

class WelcomeWindow : public QWidget {
  Q_OBJECT

 public:
  explicit WelcomeWindow(client::Client& cl, QWidget* parent = nullptr);
  ~WelcomeWindow();

 signals:
  void connectRequested(const QString& ip, int port);

 private slots:
  void onConnectClicked();

 private:
  QLineEdit* ipLineEdit;
  QLineEdit* portLineEdit;
  QPushButton* connectButton;
  QLabel* statusLabel;

  client::Client& client;

  void setupUI();
};
