#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

#include "client/client.hpp"

class AddChannelDialog : public QDialog {
  Q_OBJECT

 public:
  explicit AddChannelDialog(QWidget* parent = nullptr);
  QString getChannelName() const;

 private:
  QLineEdit* channelNameEdit;
};

class ChannelItemWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ChannelItemWidget(const QString& channelName,
                             QWidget* parent = nullptr);
  QString channelName() const;
  void setJoined(bool joined);

 signals:
  void leaveChannelRequested(const QString& channelName);

 private:
  QLabel* nameLabel;
  QPushButton* leaveButton;
  bool isJoined;
};

class MainWindow : public QWidget {
  Q_OBJECT

 public:
  explicit MainWindow(client::Client& cl, QWidget* parent = nullptr);
  ~MainWindow();

 private slots:
  void onAddChannel();
  void onRemoveChannel();
  void onChannelClicked(QListWidgetItem* item);
  void checkAndUpdateChannels();
  void onToggleMicrophone();
  void onToggleSpeaker();

 private:
  QListWidget* channelList;
  QPushButton* addButton;
  QPushButton* removeButton;
  QPushButton* micButton;
  QPushButton* speakerButton;

  client::Client& client;
  QTimer* updateTimer;

  bool isMicEnabled;
  bool isSpeakerEnabled;

  void setupUI();
  void updateChannelStyle(QListWidgetItem* item, bool selected);
  ChannelItemWidget* getChannelWidget(QListWidgetItem* item);
  void syncChannelsWithServer();
  void updateAudioButtonsStyle();
};
