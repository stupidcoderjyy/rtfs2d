//
// Created by PC on 2026/6/16.
//

#ifndef RTFS2D_OBSTACLE_EXP_PARSER_H
#define RTFS2D_OBSTACLE_EXP_PARSER_H
#include "compile/compiler.h"

namespace rtfs2d {

enum FunctionType {
    kCircle,
    kRect,
    kPoint
};

class ObstacleExpLexer : public DFALexer {
public:
    ObstacleExpLexer();
};

class CompilerInput;

class TokenInt : public Token {
public:
    int Type() override { return 129; }
    MatchResult OnMatched(const std::string& lexeme, AbstractInput& input) override;
    int data_{};
};

class TokenId : public Token{
public:
    int Type() override { return 128; }
    MatchResult OnMatched(const std::string& lexeme, AbstractInput& input) override;
    FunctionType type_{};
};

class TokenFloat : public Token{
public:
    int Type() override { return 130; }
    MatchResult OnMatched(const std::string& lexeme, AbstractInput& input) override;
    float data_{};
};

class ObstacleGeometry;

class ObstacleExpParser final : public LALRParser {
    friend class PropertyRoot;
public:
    explicit ObstacleExpParser(ObstacleGeometry& geom);
    ~ObstacleExpParser() override = default;
    const std::vector<std::string>& debug_out() const { return debug_out_; }
protected:
    void OnFailed(const std::unique_ptr<Token>& at) override;
private:
    ObstacleGeometry* geometry_;
    std::vector<std::string> debug_out_;
    void InitActions();
    void InitGoTo();
    void InitOthers();
    void InitGrammar();
};

class PropertyArgs;
class PropertyObs;
class PropertyShape;
class PropertyArg;

class PropertyArgs : public Property{
public:
    explicit PropertyArgs(std::vector<std::string>& debug_out): debug_out_(debug_out) {}
    void OnReduced(
        const std::unique_ptr<Production>& p,
        const std::vector<std::unique_ptr<Property>>& properties) override;
    std::vector<float>& args() { return args_; }
private:
    std::vector<std::string>& debug_out_;
    std::vector<float> args_;

    void Reduce0(PropertyArg& p0); //args → arg
    void Reduce1(
        PropertyArgs& p0,
        PropertyTerminal& p1,
        PropertyArg& p2); //args → args , arg
};

class PropertyObs : public Property{
public:
    explicit PropertyObs(std::vector<std::string>& debug_out): debug_out_(debug_out) {}
    void OnReduced(
        const std::unique_ptr<Production>& p,
        const std::vector<std::unique_ptr<Property>>& properties) override;
    const std::vector<FunctionType>& types() const { return types_; }
    const std::vector<std::vector<float>>& args() const { return args_; }
private:
    std::vector<std::string>& debug_out_;
    std::vector<FunctionType> types_;
    std::vector<std::vector<float>> args_;

    void Reduce0(PropertyShape& p0); //obs → shape
    void Reduce1(
        PropertyObs& p0,
        PropertyShape& p1); //obs → obs shape
};

class PropertyShape : public Property{
public:
    explicit PropertyShape(std::vector<std::string>& debug_out): debug_out_(debug_out) {}
    void OnReduced(
        const std::unique_ptr<Production>& p,
        const std::vector<std::unique_ptr<Property>>& properties) override;
    FunctionType function() const { return function_; }
    std::vector<float>& args() { return args_; }
private:
    std::vector<std::string>& debug_out_;
    FunctionType function_{};
    std::vector<float> args_;

    void Reduce0(
        PropertyTerminal& p0,
        PropertyTerminal& p1,
        PropertyArgs& p2,
        PropertyTerminal& p3); //shape → id ( args )
};

class PropertyRoot : public Property{
public:
    explicit PropertyRoot(ObstacleGeometry& geometry, std::vector<std::string>& debug_out)
        : debug_out_(debug_out), geometry_(&geometry) {}
    void OnReduced(
        const std::unique_ptr<Production>& p,
        const std::vector<std::unique_ptr<Property>>& properties) override;
private:
    std::vector<std::string>& debug_out_;
    std::vector<std::array<float, 2>> points_;
    ObstacleGeometry* geometry_;

    void Reduce0(PropertyObs& p0); //root → obs
    void AddPoints(FunctionType type, const std::vector<float>& args);
    void AddCircle(float x0, float y0, float r, float step);
    void AddRect(float x, float y, float w, float h);
    void AddPoint(float x, float y);
};

class PropertyArg : public Property{
public:
    explicit PropertyArg(std::vector<std::string>& debug_out): debug_out_(debug_out) {}
    void OnReduced(
        const std::unique_ptr<Production>& p,
        const std::vector<std::unique_ptr<Property>>& properties) override;
    const std::vector<std::string>& debug_out() const { return debug_out_; }
    float value() const { return value_; }
private:
    std::vector<std::string>& debug_out_;
    float value_{};

    void Reduce0(PropertyTerminal& p0); //arg → int
    void Reduce1(PropertyTerminal& p0); //arg → float
};

}



#endif //RTFS2D_OBSTACLE_EXP_PARSER_H
