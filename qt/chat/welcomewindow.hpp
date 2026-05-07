#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

class WelcomeWindow : public QWidget {
  Q_OBJECT

 public:
  explicit WelcomeWindow(QWidget* parent = nullptr);
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

  void setupUI();
};
