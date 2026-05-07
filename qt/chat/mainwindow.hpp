#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QDialog>
#include <QLineEdit>

class AddChannelDialog : public QDialog
{
  Q_OBJECT

 public:
  explicit AddChannelDialog(QWidget *parent = nullptr);
  QString getChannelName() const;

 private:
  QLineEdit *channelNameEdit;
};

// Кастомный виджет для элемента списка каналов
class ChannelItemWidget : public QWidget
{
  Q_OBJECT

 public:
  explicit ChannelItemWidget(const QString &channelName, QWidget *parent = nullptr);
  QString channelName() const;
  void setJoined(bool joined);

 signals:
  void leaveChannelRequested(const QString &channelName);

 private:
  QLabel *nameLabel;
  QPushButton *leaveButton;
  bool isJoined;
};

class MainWindow : public QWidget
{
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

 private slots:
  void onAddChannel();
  void onRemoveChannel();
  void onChannelClicked(QListWidgetItem *item);
  void onLeaveChannel(const QString &channelName);

 private:
  QListWidget *channelList;
  QPushButton *addButton;
  QPushButton *removeButton;

  void setupUI();
  void updateChannelStyle(QListWidgetItem *item, bool selected);
  ChannelItemWidget* getChannelWidget(QListWidgetItem *item);
};
