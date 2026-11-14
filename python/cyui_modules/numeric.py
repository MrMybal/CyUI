"""Bounded arithmetic for numeric fields. No eval, file access or Python calls."""
import ast
import math
import re


def evaluate_numeric_expression(expression, value):
    if len(expression) > 4096:
        raise ValueError('Expression is too long')
    expression = re.sub(r'(?<![\w.])(\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)[kK]\b', r'(\1*1000)', expression)
    try:
        root = ast.parse(expression.strip().replace('^', '**'), mode='eval')
    except (SyntaxError, RecursionError) as error:
        raise ValueError('Invalid expression') from error
    if sum(1 for _ in ast.walk(root)) > 4096:
        raise ValueError('Expression budget exceeded')
    def visit(node, depth=0):
        if depth > 32:
            raise ValueError('Expression is too deeply nested')
        child = lambda n: visit(n, depth+1)
        if isinstance(node, ast.Constant) and type(node.value) in (int, float):
            result = node.value
        elif isinstance(node, ast.Name) and node.id.lower() in ('value', 'pi', 'e'):
            result = {'value': value, 'pi': math.pi, 'e': math.e}[node.id.lower()]
        elif isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
            result = child(node.operand) * (-1 if isinstance(node.op, ast.USub) else 1)
        elif isinstance(node, ast.BinOp):
            a, b = child(node.left), child(node.right)
            if isinstance(node.op, ast.Add): result = a+b
            elif isinstance(node.op, ast.Sub): result = a-b
            elif isinstance(node.op, ast.Mult): result = a*b
            elif isinstance(node.op, ast.Div): result = a/b
            elif isinstance(node.op, ast.Mod): result = math.fmod(a, b)
            elif isinstance(node.op, ast.Pow): result = math.pow(a, b)
            else: raise ValueError('Unsupported operator')
        elif isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and not node.keywords:
            args = [child(n) for n in node.args]
            name = node.func.id.lower()
            unary = {'abs': abs, 'sqrt': math.sqrt, 'floor': math.floor, 'ceil': math.ceil,
                     'round': lambda n: math.copysign(math.floor(abs(n)+.5), n)}
            if name in unary and len(args) == 1: result = unary[name](*args)
            elif name in ('min', 'max') and len(args) == 2: result = (min if name == 'min' else max)(args)
            elif name == 'clamp' and len(args) == 3 and args[1] <= args[2]: result = max(args[1], min(args[2], args[0]))
            else: raise ValueError('Unknown function or invalid arguments')
        else:
            raise ValueError('Expected arithmetic or Value')
        if not math.isfinite(result):
            raise ValueError('Non-finite result')
        return result
    try:
        return visit(root.body)
    except (OverflowError, ZeroDivisionError, TypeError) as error:
        raise ValueError('Invalid arithmetic result') from error
