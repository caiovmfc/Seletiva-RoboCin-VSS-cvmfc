#ifndef PROJECT_UNIFICATION_CUSTOMPLAYER_H
#define PROJECT_UNIFICATION_CUSTOMPLAYER_H

#include "Modules/Modules.h"
#include "Modules/Processing/ProcessingUtils/ProcessingUtils.h"

class CustomPlayer : public Processing {
 public:
  CustomPlayer(int index, QThreadPool* threadPool);

 protected:
  void buildParameters(Parameters::Handler& parameters) override;
  void connectModules(const Modules* modules) override;
  void init(const Modules* modules) override;
  void update() override;
  void exec() override;

 private:
  struct Args {};
  Args args;

  struct Shared {
    SharedOptional<Frame> frame;
    SharedOptional<Robot> robot;
    SharedOptional<Field> field;
    SharedValue<QSet<Qt::Key>> keys;
  };
  // ponto que guarda a posição mais recente do goleiro
  Point posiGY = {0, 0};
  //
  // variável que controla o número de iterações onde o goleiro não se moveu
  // para verificar se ele está preso
  int presoGy = 0;
  //
  // ponto que guarda a posição mais recente do meia
  Point posiMY = {0, 0};
  //
  // variável que controla o número de iterações onde o meia não se moveu
  // para verificar se ele está preso
  int presoMy = 0;
  //
  // ponto que guarda a posição mais recente do atacante
  Point posiAY = {0, 0};
  //
  // variável que controla o número de iterações onde o atacante não se moveu
  // para verificar se ele está preso
  int presoAy = 0;
  //
  SharedWrapper<Shared, std::mutex> shared;

  std::optional<Field> field;
  std::optional<Frame> frame;
  std::optional<Robot> robot;

  SSLNavigation sslNavigation;
  VSSNavigation vssNavigation;

 private slots:
  void receiveField(const Field& field);
  void receiveFrame(const Frame& frame);
};

#endif // PROJECT_UNIFICATION_CUSTOMPLAYER_H