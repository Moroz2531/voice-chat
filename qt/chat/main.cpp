#include <QApplication>

#include "welcomewindow.hpp"
#include "client/client.hpp"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  client::Client cl;
  WelcomeWindow welcomeWindow{cl};
  welcomeWindow.show();

  return app.exec();
}
