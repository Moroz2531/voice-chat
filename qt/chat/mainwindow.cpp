#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

#include <QSet>
#include <QTimer>

#include "mainwindow.hpp"

// Реализация диалога добавления канала
AddChannelDialog::AddChannelDialog(QWidget* parent) : QDialog(parent) {
  /*setWindowTitle("Добавить канал");
  setFixedSize(300, 120);

  QVBoxLayout* layout = new QVBoxLayout(this);

  QLabel* label = new QLabel("Введите название канала:");
  layout->addWidget(label);

  channelNameEdit = new QLineEdit();
  channelNameEdit->setPlaceholderText("Название канала");
  layout->addWidget(channelNameEdit);

  QHBoxLayout* buttonLayout = new QHBoxLayout();
  QPushButton* okButton = new QPushButton("OK");
  QPushButton* cancelButton = new QPushButton("Отмена");

  buttonLayout->addWidget(okButton);
  buttonLayout->addWidget(cancelButton);
  layout->addLayout(buttonLayout);

  connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);*/
}

QString AddChannelDialog::getChannelName() const {
  return channelNameEdit->text().trimmed();
}

// Реализация кастомного виджета для элемента списка
ChannelItemWidget::ChannelItemWidget(const QString& channelName,
                                     QWidget* parent)
    : QWidget(parent), isJoined(false) {
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(5, 2, 5, 2);
  layout->setSpacing(5);

  nameLabel = new QLabel(channelName);
  nameLabel->setStyleSheet("font-size: 14px; padding: 5px;");
  layout->addWidget(nameLabel);

  layout->addStretch();

  leaveButton = new QPushButton("✕");
  leaveButton->setFixedSize(30, 30);
  leaveButton->setStyleSheet(
      "QPushButton {"
      "   background-color: #f44336;"
      "   color: white;"
      "   border: none;"
      "   border-radius: 15px;"
      "   font-size: 16px;"
      "   font-weight: bold;"
      "}"
      "QPushButton:hover {"
      "   background-color: #d32f2f;"
      "}");
  leaveButton->setVisible(false);
  leaveButton->setToolTip("Покинуть канал");

  connect(leaveButton, &QPushButton::clicked, this,
          [this]() { emit leaveChannelRequested(nameLabel->text()); });

  layout->addWidget(leaveButton);
}

QString ChannelItemWidget::channelName() const {
  return nameLabel->text();
}

void ChannelItemWidget::setJoined(bool joined) {
  isJoined = joined;
  leaveButton->setVisible(joined);

  if (joined) {
    this->setStyleSheet(
        "background-color: #2E7D32;"
        "border-radius: 4px;");
    nameLabel->setStyleSheet(
        "color: white; font-size: 14px; padding: 5px; font-weight: bold;");
  } else {
    this->setStyleSheet("");
    nameLabel->setStyleSheet("color: black; font-size: 14px; padding: 5px;");
  }
}

// Реализация основного окна
MainWindow::MainWindow(client::Client& cl, QWidget* parent)
    : QWidget(parent), client{cl}, isMicEnabled(true), isSpeakerEnabled(true) {
  setupUI();
  setWindowTitle("Каналы");
  setMinimumSize(500, 400);
  resize(600, 500);

  updateTimer = new QTimer(this);
  connect(updateTimer, &QTimer::timeout, this,
          &MainWindow::checkAndUpdateChannels);
  updateTimer->start(500);
}

MainWindow::~MainWindow() {}

void MainWindow::checkAndUpdateChannels() {
  if (client.sizeChannels() != static_cast<size_t>(channelList->count())) {
    syncChannelsWithServer();
  }
}

void MainWindow::syncChannelsWithServer() {
  // Получаем актуальные данные с сервера
  const auto& serverChannels = client.channels();

  // Создаем множества для сравнения
  QList<QString> localChannels;
  QList<QString> serverChannelsSet;

  // Собираем локальные каналы
  for (int i = 0; i < channelList->count(); ++i) {
    QListWidgetItem* item = channelList->item(i);
    ChannelItemWidget* widget = getChannelWidget(item);
    if (widget) {
      localChannels.push_back(widget->channelName());
    }
  }

  // Собираем серверные каналы (зависит от вашей структуры ServerData)
  for (const auto& channel : serverChannels) {
    serverChannelsSet.push_back(
        QString::number(channel));  // Предполагаемое поле
  }

  // Проверяем различия
  if (localChannels != serverChannelsSet) {
    channelList->blockSignals(true);

    // Очищаем текущий список
    channelList->clear();

    // Добавляем все каналы с сервера
    for (const auto& channel : serverChannelsSet) {
      QListWidgetItem* item = new QListWidgetItem();
      item->setSizeHint(QSize(0, 50));

      ChannelItemWidget* channelWidget = new ChannelItemWidget(channel);

      channelList->addItem(item);
      channelList->setItemWidget(item, channelWidget);
    }

    // Разблокируем сигналы
    channelList->blockSignals(false);
  }
}

void MainWindow::setupUI() {
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(10);
  mainLayout->setContentsMargins(15, 15, 15, 15);

  // Верхняя панель с кнопками
  QHBoxLayout* topPanelLayout = new QHBoxLayout();

  QLabel* titleLabel = new QLabel("Список каналов");
  titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
  topPanelLayout->addWidget(titleLabel);

  topPanelLayout->addStretch();

  addButton = new QPushButton("+ Добавить канал");
  addButton->setStyleSheet(
      "QPushButton {"
      "   background-color: #2196F3;"
      "   color: white;"
      "   padding: 8px 15px;"
      "   border-radius: 4px;"
      "   font-weight: bold;"
      "}"
      "QPushButton:hover {"
      "   background-color: #1976D2;"
      "}");

  removeButton = new QPushButton("- Удалить канал");
  removeButton->setStyleSheet(
      "QPushButton {"
      "   background-color: #f44336;"
      "   color: white;"
      "   padding: 8px 15px;"
      "   border-radius: 4px;"
      "   font-weight: bold;"
      "}"
      "QPushButton:hover {"
      "   background-color: #d32f2f;"
      "}");

  topPanelLayout->addWidget(addButton);
  topPanelLayout->addWidget(removeButton);

  mainLayout->addLayout(topPanelLayout);

  // Панель управления аудио устройствами
  QHBoxLayout* audioPanelLayout = new QHBoxLayout();

  // Кнопка микрофона
  micButton = new QPushButton("🎤 Микрофон вкл");
  micButton->setCheckable(true);
  micButton->setChecked(true);
  micButton->setStyleSheet(
      "QPushButton {"
      "   background-color: #4CAF50;"
      "   color: white;"
      "   padding: 10px 20px;"
      "   border-radius: 4px;"
      "   font-weight: bold;"
      "   font-size: 14px;"
      "}"
      "QPushButton:hover {"
      "   background-color: #45a049;"
      "}"
      "QPushButton:checked {"
      "   background-color: #f44336;"
      "}"
      "QPushButton:checked:hover {"
      "   background-color: #d32f2f;"
      "}");

  // Кнопка динамиков
  speakerButton = new QPushButton("🔊 Динамики вкл");
  speakerButton->setCheckable(true);
  speakerButton->setChecked(true);
  speakerButton->setStyleSheet(
      "QPushButton {"
      "   background-color: #4CAF50;"
      "   color: white;"
      "   padding: 10px 20px;"
      "   border-radius: 4px;"
      "   font-weight: bold;"
      "   font-size: 14px;"
      "}"
      "QPushButton:hover {"
      "   background-color: #45a049;"
      "}"
      "QPushButton:checked {"
      "   background-color: #f44336;"
      "}"
      "QPushButton:checked:hover {"
      "   background-color: #d32f2f;"
      "}");

  audioPanelLayout->addWidget(micButton);
  audioPanelLayout->addWidget(speakerButton);
  audioPanelLayout->addStretch();

  mainLayout->addLayout(audioPanelLayout);

  // Список каналов
  channelList = new QListWidget();
  channelList->setStyleSheet(
      "QListWidget {"
      "   border: 2px solid #ccc;"
      "   border-radius: 5px;"
      "   padding: 5px;"
      "   font-size: 14px;"
      "}"
      "QListWidget::item {"
      "   padding: 0px;"
      "   margin: 2px;"
      "   border-radius: 4px;"
      "   background-color: #f5f5f5;"
      "}"
      "QListWidget::item:hover {"
      "   background-color: #e0e0e0;"
      "}");

  channelList->setSelectionMode(QAbstractItemView::SingleSelection);

  mainLayout->addWidget(channelList);

  // Подключаем сигналы
  connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddChannel);
  connect(removeButton, &QPushButton::clicked, this,
          &MainWindow::onRemoveChannel);
  connect(channelList, &QListWidget::itemClicked, this,
          &MainWindow::onChannelClicked);
  connect(micButton, &QPushButton::clicked, this,
          &MainWindow::onToggleMicrophone);
  connect(speakerButton, &QPushButton::clicked, this,
          &MainWindow::onToggleSpeaker);
}

void MainWindow::onAddChannel() {
  client.insertChannel();
  /*AddChannelDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    QString channelName = dialog.getChannelName();
    if (!channelName.isEmpty()) {
      // Создаем элемент списка
      QListWidgetItem* item = new QListWidgetItem();
      item->setSizeHint(QSize(0, 50)); // Устанавливаем высоту элемента

      // Создаем кастомный виджет для этого элемента
      ChannelItemWidget* channelWidget = new ChannelItemWidget(channelName);

      // Подключаем сигнал покидания канала
      connect(channelWidget, &ChannelItemWidget::leaveChannelRequested,
              this, &MainWindow::onLeaveChannel);

      // Добавляем элемент в список
      channelList->addItem(item);
      channelList->setItemWidget(item, channelWidget);
    }
  }*/
}

void MainWindow::onRemoveChannel() {
  int currentRow = channelList->currentRow();
  if (currentRow >= 0) {
    QListWidgetItem* item = channelList->item(currentRow);

    ChannelItemWidget* channelWidget = getChannelWidget(item);

    if (channelWidget) {
      QString channelName = channelWidget->channelName();

      bool ok;
      auto id = channelName.toULongLong(&ok);

      if (ok) {
        client.eraseChannel(id);
      } else {
        QMessageBox::information(
            this, "Ошибка", "Невозможно преобразовать имя канала в число.");
      }
    } else {
      QMessageBox::information(this, "Ошибка",
                               "Не удалось получить виджет канала.");
    }
  } else {
    QMessageBox::information(this, "Удаление канала",
                             "Пожалуйста, выберите канал для удаления.");
  }
}

void MainWindow::onChannelClicked(QListWidgetItem* item) {
  ChannelItemWidget* channelWidget = getChannelWidget(item);
  if (channelWidget) {
    // Переключаем состояние "присоединен/отсоединен"
    bool isCurrentlyJoined =
        channelWidget->findChild<QPushButton*>()->isVisible();
    channelWidget->setJoined(!isCurrentlyJoined);

    // Здесь можно добавить вашу логику подключения/отключения от канала
    if (!isCurrentlyJoined) {
      // Присоединяемся к каналу
      QString channelName = channelWidget->channelName();
      bool ok;
      auto id = channelName.toULongLong(&ok);

      if (ok) {
        client.connectStream(id);
      } else {
        QMessageBox::information(
            this, "Ошибка", "Невозможно преобразовать имя канала в число.");
      }
    } else {
      client.disconnectStream();
    }
  }
}

void MainWindow::onToggleMicrophone() {
  isMicEnabled = !isMicEnabled;

  if (isMicEnabled) {
    micButton->setText("🎤 Микрофон вкл");
    client.runInputDevice();
  } else {
    micButton->setText("🎤 Микрофон выкл");
    client.stopInputDevice();
  }
}

void MainWindow::onToggleSpeaker() {
  isSpeakerEnabled = !isSpeakerEnabled;

  if (isSpeakerEnabled) {
    speakerButton->setText("🔊 Динамики вкл");
    client.runOutputDevice();
  } else {
    speakerButton->setText("🔊 Динамики выкл");
    client.stopOutputDevice();
  }
}

ChannelItemWidget* MainWindow::getChannelWidget(QListWidgetItem* item) {
  return qobject_cast<ChannelItemWidget*>(channelList->itemWidget(item));
}

void MainWindow::updateChannelStyle(QListWidgetItem* item, bool selected) {
  ChannelItemWidget* widget = getChannelWidget(item);
  if (widget) {
    widget->setJoined(selected);
  }
}

void MainWindow::updateAudioButtonsStyle() {
  // Обновление стиля кнопки микрофона
  if (isMicEnabled) {
    micButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   padding: 10px 20px;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45a049;"
        "}");
  } else {
    micButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #f44336;"
        "   color: white;"
        "   padding: 10px 20px;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #d32f2f;"
        "}");
  }

  // Обновление стиля кнопки динамиков
  if (isSpeakerEnabled) {
    speakerButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   padding: 10px 20px;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45a049;"
        "}");
  } else {
    speakerButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #f44336;"
        "   color: white;"
        "   padding: 10px 20px;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #d32f2f;"
        "}");
  }
}
