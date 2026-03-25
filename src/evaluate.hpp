#ifndef EVALUATE_HPP
#define EVALUATE_HPP


struct Evaluate{

    Board board;
    int materialScore;
    int positionScore;
    int totalScore;

    Evaluate(const Board& board);
    void materialBalance();
    void positionBalance();
    void evaluateTotal();
};



#endif
