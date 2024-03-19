#!/usr/bin/env wolframscript

(* Function to evaluate the expression *)
evaluate[expr_] := Module[{result},
 result = ToExpression[expr, TeXForm];
 (*Print[NumberQ[result]]
 Print[ToString[result]]*)
 
 If[NumberQ[result],
    result,
    ToString @ TeXForm[SemanticInterpretation[ToString[N[result,4],InputForm]]]
 ]
];

(* Main script execution *)
If[Length[$ScriptCommandLine] > 1,
 Print[evaluate[$ScriptCommandLine[[2]]]],
 Print["Please provide an equation as a command-line argument."]
];
