#include "App.h"
#include "Config.h"

void App::begin() {
  network_.begin();
  link_.begin();
  web_.begin();
}

void App::update() {
  link_.update(network_.enabled(), network_.status());
  network_.setEnabled(link_.wifiRequested());
  web_.update(network_.enabled());
  delay(1);
}
