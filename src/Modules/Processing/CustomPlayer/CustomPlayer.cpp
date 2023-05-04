#include "CustomPlayer.h"
#include "Modules/Processing/MouseAndKeyboard/MouseAndKeyboard.h"
#include "Packages/VSSRobotCommand/VSSRobotCommand.h"
#include "soccer-common/Extends/QPoint/ExtendsQPoint.h"

CustomPlayer::CustomPlayer(int index, QThreadPool* threadPool) : Processing(index, threadPool) {
}

void CustomPlayer::buildParameters(Parameters::Handler& parameters) {
}

void CustomPlayer::connectModules(const Modules* modules) {
  connect(modules->vision(),
          &Vision::sendFrame,
          this,
          &CustomPlayer::receiveFrame,
          Qt::DirectConnection);

  connect(modules->vision(),
          &Vision::sendField,
          this,
          &CustomPlayer::receiveField,
          Qt::DirectConnection);
}

void CustomPlayer::init(const Modules* modules) {
}

void CustomPlayer::update() {
  shared->field.extract_to(field);
  if (auto f = shared->frame.get_optional_and_reset()) {
    if (auto it = f->allies().findById(index()); it != f->allies().end()) {
      robot = *it;
    }
    frame.emplace(*f);
  }
}

/*
Point posiGY = {0, 0}; esses pontos e contadores estão declarados e explicados no CustomPlayer.h
int presoGy = 0;
Point posiMY = {0, 0};
int presoMy = 0;
Point posiAY = {0 , 0};
int presoAy = 0;
/
int x = 1; variáveis que usei
int y = 2; para debugar
*/

void CustomPlayer::exec() {
  if (!field || !frame || !robot) {
    return;
  }

  // TODO: here...
  // emit sendCommand(...);
  auto ball = frame->ball();
  auto allies = frame->allies();
  auto enemies = frame->enemies();
  auto center = field->center();

  // foquei em configurar somente um dos times (amarelo, no caso)
  // recomendo retirar o time azul de campo na simulação
  auto atacanteY = allies.findById(0); // robô de id 0 joga de atacante
  auto meiaY = allies.findById(1);     // robô de id 1 joga de meio-campo
  auto goleiroY = allies.findById(2);  // robô de id 2 joga de goleiro
  /*
  auto atacanteB = enemies.findById(0);
  auto meiaB = enemies.findById(1);
  auto goleiroB = enemies.findById(2);
  */

  /*

  Observações para ajudar a entender a implementação escolhida e o código:

  - Cada jogador tem uma função específica e joga em uma zona delimitada:
    * O goleiro na pequena área
    * O meia no campo aliado (entrando só um pouco no campo de ataque e na área do goleiro)
    * O atacante no campo de ataque

  - O meia e atacante invertem seus posicionamentos em relação ao y da bola a depender dela
    estar ou não em suas zonas. O objetivo disso é fazer com que esses 2 jogadores, em conjunto,
    não deixem nenhum dos lados do campo exposto/desprotegido.

  - Se um jogador não se mover ou se mover muito pouco por 15 iterações ou mais,
    é considerado que ele ficou preso

  - Antes de cada movimento ser chamado, é feita uma verificação para garantir que o
    jogador em questão não esteja preso justamente devido a esse movimento. Dessa forma,
    o único movimento permitido para um robô preso é o de desprender (spins são a única exceção,
    pois não levam o robô a ficar preso)

  - O atacante é mais criterioso que o meia ao chutar, para que aumente as chances de gol

  - Pela implementação que escolhi, só havia necessidade da função de desvio para o meia em
    relação ao goleiro (não ocorrem colisões meia/atacante e nem goleiro/atacante), mas ela
    poderia ser adaptada para todos os casos

  */

  // ir para o centro
  VSSMotion::GoToPoint GoToCenter(field->center());
  VSSRobotCommand GTC(GoToCenter);

  // girar no sentido horário
  VSSMotion::Spin SpinH(false);
  VSSRobotCommand SH(SpinH);

  // girar no sentido anti-horário
  VSSMotion::Spin SpinAH(true);
  VSSRobotCommand SAH(SpinAH);

  // ir na direção da bola
  VSSMotion::GoToPoint GoToBall(ball.position());
  VSSRobotCommand GTB(GoToBall);

  // parar
  VSSMotion::Stop Stop;
  VSSRobotCommand STOP(Stop);

  // ir para o canto esquerdo do gol (goleiro)
  Point LeftGY = Point(-68.5, 17);
  VSSMotion::GoToPoint GoToLeftGY(LeftGY);
  VSSRobotCommand GTLGY(GoToLeftGY);

  // ir para o canto direito do gol (goleiro)
  Point RightGY = Point(-68.5, -17);
  VSSMotion::GoToPoint GoToRightGY(RightGY);
  VSSRobotCommand GTRGY(GoToRightGY);

  // acompanhar a bola em sua coordenada y sem sair dos limites do gol (goleiro)
  Point golY = Point(-67, ball.y());
  VSSMotion::GoToPoint GYShadowBall(golY);
  VSSRobotCommand GYSB(GYShadowBall);

  // posicionar-se atrás da bola (meia/atacante)
  Point BackBall = Point(ball.x() - 60, ball.y());
  VSSMotion::GoToPoint GoToBackBall(BackBall);
  VSSRobotCommand GTBB(GoToBackBall);

  // recuar se a bola estiver com o atacante e inverter posicionamento em relação à bola (meia)
  Point BackFieldY = Point(-40, (-0.66) * ball.y());
  VSSMotion::GoToPoint BackFY(BackFieldY);
  VSSRobotCommand BFY(BackFY);

  // avançar se a bola estiver com o meia e inverter posicionamento em relação à bola (atacante)
  Point FrontFieldY = Point(15, (-0.66) * ball.y());
  VSSMotion::GoToPoint FrontFY(FrontFieldY);
  VSSRobotCommand FFY(FrontFY);

  // desviar do goleiro se afastando horizontalmente
  // enquanto vai em direção ao y vezes 2 da bola (meia)
  Point DesviarY = Point(50, (2) * ball.y());
  VSSMotion::GoToPoint DesvY(DesviarY);
  VSSRobotCommand DY(DesvY);

  // TIME AMARELO

  // se a bola entrar no gol aliado ou adversário, todos os robôs param
  if (ball.x() < -75 || ball.x() > 75) {
    emit sendCommand(vssNavigation.run(*robot, STOP));
  }
  // se a bola não estiver dentro de nenhum dos gols, o time joga normalmente
  else {

    /*
    GOLEIRO
    */
    // qDebug() << posiGY;
    //
    //  CONTROLANDO POR QUANTAS ITERAÇÔES O GOLEIRO NAO SE MOVEU E SE ELE ESTA PRESO
    // é feita uma comparação da posição atual do goleiro com sua última posição armazenada e a
    // cada iteração que o goleiro não se mover (ou se mover muito pouco), incremento a variável
    // "presoGy"
    if (posiGY.x() > goleiroY->x() - 1.7 && posiGY.x() < goleiroY->x() + 1.7 &&
        posiGY.y() > goleiroY->y() - 1.7 && posiGY.y() < goleiroY->y() + 1.7) {
      presoGy++;
    }
    // caso o goleiro se mova, a posição dele é atualizada e a variável presoGy zerada
    else {
      posiGY = goleiroY->position();
      presoGy = 0;
      // qDebug() << presoGy;
    }
    // MOVIMENTOS
    // verificando se a bola está próxima o suficiente do goleiro para que ele defenda girando
    if (ball.x() - goleiroY->x() <= 7 && ball.x() - goleiroY->x() >= -5 &&
        ball.y() - goleiroY->y() <= 7 && ball.y() - goleiroY->y() >= -7) {
      // se a bola estiver acima do goleiro, gira no sentido horario
      if (ball.y() > goleiroY->y()) {
        emit sendCommand(vssNavigation.run(*goleiroY, SH));
      }
      // se a bola estiver abaixo do goleiro, gira no sentido anti-horario
      else {
        emit sendCommand(vssNavigation.run(*goleiroY, SAH));
      }
    }
    /* goleiro para se estiver corretamente posicionado em relação à coordenada y da bola ou se
    estiver em algum dos cantos do gol e com a bola posicionada além desses cantos */
    else if ((goleiroY->y() < ball.y() + 3 && goleiroY->y() > ball.y() - 3) ||
             (ball.y() >= 19 && goleiroY->y() > 18) || (ball.y() <= -19 && goleiroY->y() < -18)) {
      emit sendCommand(vssNavigation.run(*goleiroY, STOP));
      // posição atualizada e presoGy zerado pois nesse caso o goleiro
      // não está preso, mas sim parado de propósito
      posiGY = goleiroY->position();
      presoGy = 0;
    }
    // caso contrário, se movimenta buscando seguir a "sombra" da bola
    else {
      // se estiver preso, desprende indo para o meio
      if (presoGy >= 15) {
        emit sendCommand(vssNavigation.run(*goleiroY, GTC));
      }
      // se não estiver preso, vai "seguir" a bola (somente no eixo y)
      else {
        emit sendCommand(vssNavigation.run(*goleiroY, GYSB));
      }
    }
    // caso a bola exceda um certo limite superior no eixo y, goleiro se posiciona no canto esquerdo
    if (ball.y() >= 19) {
      // se estiver preso, desprende indo para o meio
      if (presoGy >= 15) {
        emit sendCommand(vssNavigation.run(*goleiroY, GTC));
      }
      // se não estiver preso, vai para o canto esquerdo
      else {
        emit sendCommand(vssNavigation.run(*goleiroY, GTLGY));
      }
    }
    // caso a bola exceda um certo limite inferior no eixo y, goleiro se posiciona no canto direito
    else if (ball.y() <= -19) {
      // se estiver preso, desprende indo para o meio
      if (presoGy >= 15) {
        emit sendCommand(vssNavigation.run(*goleiroY, GTC));
      }
      // se não estiver preso, vai para o canto direito
      else {
        emit sendCommand(vssNavigation.run(*goleiroY, GTRGY));
      }
    }

    /*
    MEIA
    */
    // qDebug() << posiMY;
    //
    //  CONTROLANDO POR QUANTAS ITERAÇÔES O MEIA NAO SE MOVEU E SE ELE ESTA PRESO
    // é feita uma comparação da posição atual do meia com sua última posição armazenada e a cada
    // iteração que o meia não se mover (ou se mover muito pouco), incremento a variável "presoMy"
    if (posiMY.x() > meiaY->x() - 1.7 && posiMY.x() < meiaY->x() + 1.7 &&
        posiMY.y() > meiaY->y() - 1.7 && posiMY.y() < meiaY->y() + 1.7) {
      presoMy++;
      /* if (presoMy >= 15) {
        if (meiaY->x() < -20 || meiaY->y() < -20 || meiaY->y() > 20) {
        emit sendCommand(vssNavigation.run(*meiaY, GTC));
        qDebug() << presoMy;
        }
      } */
    }
    // caso o meia se mova, a posição dele é atualizada e a variável presoMy zerada
    else {
      posiMY = meiaY->position();
      presoMy = 0;
      // qDebug() << presoMy;
    }
    // MOVIMENTOS
    // caso a bola não esteja na zona do meia, ele permanece no meio
    // do campo e no lado contrário da bola
    if (ball.x() > 20 || (ball.x() < -65 && ball.y() > -22 && ball.y() < 22)) {
      emit sendCommand(vssNavigation.run(*meiaY, BFY));
    }
    // sobra a área na qual o meia joga
    else {
      // verificando se a bola está próxima o suficiente para chutar
      if (ball.x() - meiaY->x() <= 7 && ball.x() - meiaY->x() >= -5 && ball.y() - meiaY->y() <= 7 &&
          ball.y() - meiaY->y() >= -7) {
        // se a bola estiver acima do meia, gira no sentido horario
        if (ball.y() > meiaY->y()) {
          emit sendCommand(vssNavigation.run(*meiaY, SH));
        }
        // se a bola estiver abaixo do meia, gira no sentido anti-horario
        else {
          emit sendCommand(vssNavigation.run(*meiaY, SAH));
        }
      }
      // verificando se o meia está prestes a colidir com o goleiro
      // caso sim, desvia indo para o centro do campo ao mesmo tempo que
      // ainda segue o rumo da bola (pela sua coordenada y*2)
      else if (meiaY->x() < goleiroY->x() + 10 && meiaY->x() > goleiroY->x() - 10 &&
               meiaY->y() < goleiroY->y() + 10 && meiaY->y() > goleiroY->y() - 10) {
        // verificando se o meia ainda precisa desviar mais
        // (caso ainda não tenha passado do goleiro)
        if ((ball.y() < goleiroY->y() && meiaY->y() > goleiroY->y()) ||
            (ball.y() > goleiroY->y() && meiaY->y() < goleiroY->y())) {
          emit sendCommand(vssNavigation.run(*meiaY, DY));
        }
        // se ja tiver desviado, vai na bola
        else {
          emit sendCommand(vssNavigation.run(*meiaY, GTB));
        }
      }
      // verificando se o meia está na frente da bola (se sim, vai para atrás dela).
      // a segunda condição nesse caso busca evitar que o meia tente ir para
      // atrás da bola se ela estiver muito próxima a uma parede, o que poderia travá-lo
      else if (meiaY->x() > ball.x() + 3 && ball.x() > -63) {
        // qDebug() << x;
        // se estiver preso, desprende indo para o meio
        if (presoMy >= 15) {
          emit sendCommand(vssNavigation.run(*meiaY, GTC));
        }
        // se não estiver preso, vai para atrás da bola
        else {
          emit sendCommand(vssNavigation.run(*meiaY, GTBB));
        }
      }
      // verificando se o meia está em posição favorável para ir na bola
      else {
        // qDebug() << y;
        // se estiver preso, desprende indo para o meio
        if (presoMy >= 15) {
          emit sendCommand(vssNavigation.run(*meiaY, GTC));
        }
        // se não estiver preso, vai na bola
        else {
          emit sendCommand(vssNavigation.run(*meiaY, GTB));
        }
      }
    }

    /*
    ATACANTE
    */
    // qDebug() << posiAY;
    //
    // CONTROLANDO POR QUANTAS ITERAÇÔES O ATACANTE NAO SE MOVEU E SE ELE ESTA PRESO
    // é feita uma comparação da posição atual do atacante com sua última posição armazenada e a
    // cada iteração que o atacante não se mover (ou se mover muito pouco), incremento a variável
    // "presoAy"
    if (posiAY.x() > atacanteY->x() - 1.7 && posiAY.x() < atacanteY->x() + 1.7 &&
        posiAY.y() > atacanteY->y() - 1.7 && posiAY.y() < atacanteY->y() + 1.7) {
      presoAy++;
    }
    // caso o atacante se mova, a posição dele é atualizada e a variável presoAy zerada
    else {
      posiAY = atacanteY->position();
      presoAy = 0;
      // qDebug() << presoAy;
    }
    // MOVIMENTOS
    // delimitando a área na qual o atacante joga
    if (ball.x() > 20) {
      // verificando se a bola está próxima o suficiente para chutar
      if (ball.x() - atacanteY->x() <= 8 && ball.x() - atacanteY->x() >= -5 &&
          ball.y() - atacanteY->y() <= 7 && ball.y() - atacanteY->y() >= -7) {
        // se a bola estiver na parte de cima do campo
        if (ball.y() > 0) {
          // se a bola estiver alinhada com o gol e abaixo do atacante, gira em anti-horário
          if (ball.y() < atacanteY->y() && ball.y() < 26) {
            emit sendCommand(vssNavigation.run(*atacanteY, SAH));
          }
          // caso não, gira no sentido horário
          else {
            emit sendCommand(vssNavigation.run(*atacanteY, SH));
          }
        }
        // se a bola estiver na parte de baixo do campo
        else {
          // se a bola estiver alinhada com o gol e acima do atacante, gira em sentido horário
          if (ball.y() > atacanteY->y() && ball.y() > -26) {
            emit sendCommand(vssNavigation.run(*atacanteY, SH));
          }
          // caso não, gira no sentido anti-horário
          else {
            emit sendCommand(vssNavigation.run(*atacanteY, SAH));
          }
        }
      }
      // verificando se o atacante está na frente da bola
      else if (atacanteY->x() > ball.x() + 1) {
        // se estiver preso, desprende indo para o meio
        if (presoAy >= 15) {
          emit sendCommand(vssNavigation.run(*atacanteY, GTC));
        }
        // se não estiver preso, vai para atrás da bola
        else {
          emit sendCommand(vssNavigation.run(*atacanteY, GTBB));
        }
      }
      // verificando se o atacante está em posição favorável
      else {
        // se estiver preso, desprende indo para o meio
        if (presoAy >= 15) {
          emit sendCommand(vssNavigation.run(*atacanteY, GTC));
        }
        // se não estiver preso, vai na bola
        else {
          emit sendCommand(vssNavigation.run(*atacanteY, GTB));
        }
      }
    }
    /* caso a bola não esteja na zona de atuação do atacante, permanece um pouco na frente
    (posicionado no lado invertido da bola) esperando a jogada do meia */
    else {
      emit sendCommand(vssNavigation.run(*atacanteY, FFY));
    }
  }
  //
}

void CustomPlayer::receiveField(const Field& field) {
  shared->field = field;
}

void CustomPlayer::receiveFrame(const Frame& frame) {
  shared->frame = frame;
  runInParallel();
}

static_block {
  Factory::processing.insert<CustomPlayer>();
};