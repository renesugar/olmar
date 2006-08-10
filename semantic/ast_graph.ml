
open Cc_ml_types
open Cc_ast_gen_type
open Ml_ctype
open Ast_annotation
open Ast_util




(* node type done: 7, 8, 9, 11, 12, 13, 14, 15, 16, 19, 20, 
 *  21, 22, 23, 24, 25, 26, 38, 39, 40
 *)

let oc = ref stdout;;

let print_caddr = ref false


let not_implemented () =
  if 0 = 0 then 0 else assert false

(**************************************************************************
 *
 * contents of astiter.ml with sourceLoc_fun removed
 *
 **************************************************************************)


(* 
 * 
 * let opt_iter f = function
 *   | None -> ()
 *   | Some x -> f x
 *)

let any_label (field_name, value) =
  Printf.fprintf !oc "\\n%s: %s" field_name value

(* let int_fun (i : int) = ()
 *)

let string_opt = function
  | None -> "(nil)"
  | Some s -> s

(* let sourceLoc_fun((file : string), (line : int), (char : int)) = ()
 *)

(* let variable_fun(v : annotated variable) = ()
 * 
 * let cType_fun(c : annotated cType) = ()
 * 
 * let accessKeyword_fun(keyword : accessKeyword) = ()
 *)

(* let overloadableOp_fun(op :overloadableOp) = ()
 * 
 * let unaryOp_fun(op : unaryOp) = ()
 * 
 * let effectOp_fun(op : effectOp) = ()
 * 
 * let binaryOp_fun(op : binaryOp) = ()
 * 
 * let castKeyword_fun(keyword : castKeyword) = ()
 * 
 * let function_flags_fun(flags : function_flags) = ()
 * 
 * 
 * let array_size_fun = function
 *   | NO_SIZE -> ()
 *   | DYN_SIZE -> ()
 *   | FIXED_SIZE(int) -> int_fun int
 * 
 * let compoundType_Keyword_fun = function
 *   | K_STRUCT -> ()
 *   | K_CLASS -> ()
 *   | K_UNION -> ()
 * 
 * 
 * (\***************** variable ***************************\)
 * 
 * 1 let rec variable_fun(v : annotated variable) =
 *   sourceLoc_fun v.loc;
 *   opt_iter string_fun v.var_name;
 * 
 *   (\* POSSIBLY CIRCULAR *\)
 *   opt_iter cType_fun !(v.var_type);
 *   declFlags_fun v.flags;
 *   opt_iter expression_fun v.value;
 *   opt_iter cType_fun v.defaultParam;
 * 
 *   (\* POSSIBLY CIRCULAR *\)
 *   opt_iter func_fun !(v.funcDefn)
 * 
 * 
 * (\***************** cType ******************************\)
 * 
 * 2 and baseClass_fun baseClass =
 *   compound_info_fun baseClass.compound;
 *   accessKeyword_fun baseClass.bc_access;
 *   bool_fun baseClass.is_virtual
 * 
 * 
 * 3 and compound_info_fun info = 
 *   string_fun info.compound_name;
 *   variable_fun info.typedef_var;
 *   accessKeyword_fun info.ci_access;
 *   bool_fun info.is_forward_decl;
 *   compoundType_Keyword_fun info.keyword;
 *   List.iter variable_fun info.data_members;
 *   List.iter baseClass_fun info.bases;
 *   List.iter variable_fun info.conversion_operators;
 *   List.iter variable_fun info.friends;
 *   string_fun info.inst_name;
 * 
 *   (\* POSSIBLY CIRCULAR *\)
 *   opt_iter cType_fun !(info.self_type)
 * 
 * 
 * 4 and atomicType_fun = function
 *   | SimpleType(annot, simpleTypeId) ->
 *       simpleTypeId_fun simpleTypeId
 * 
 *   | CompoundType(compound_info) ->
 *       compound_info_fun compound_info
 * 
 *   | PseudoInstantiation(annot, str, variable_opt, accessKeyword, 
 * 			compound_info, sTemplateArgument_list) ->
 *       string_fun str;
 *       opt_iter variable_fun variable_opt;
 *       accessKeyword_fun accessKeyword;
 *       compound_info_fun compound_info;
 *       List.iter sTemplateArgument_fun sTemplateArgument_list
 * 
 *   | EnumType(annot, string, variable, accessKeyword, string_int_list) ->
 *       string_fun string;
 *       variable_fun variable;
 *       accessKeyword_fun accessKeyword;
 *       List.iter (fun (string, int) -> 
 * 		  (string_fun string; int_fun int))
 * 	string_int_list
 * 
 *   | TypeVariable(annot, string, variable, accessKeyword) ->
 *       string_fun string;
 *       variable_fun variable;
 *       accessKeyword_fun accessKeyword
 * 
 * 
 * 5 and cType_fun = function
 *   | CVAtomicType(annot, cVFlags, atomicType) ->
 *       cVFlags_fun cVFlags;
 *       atomicType_fun atomicType
 * 
 *   | PointerType(annot, cVFlags, cType) ->
 *       cVFlags_fun cVFlags;
 *       cType_fun cType
 * 
 *   | ReferenceType(annot, cType) ->
 *       cType_fun cType
 * 
 *   | FunctionType(annot, function_flags, cType, variable_list, cType_list_opt) ->
 *       function_flags_fun function_flags;
 *       cType_fun cType;
 *       List.iter variable_fun variable_list;
 *       opt_iter (List.iter cType_fun) cType_list_opt
 * 
 *   | ArrayType(annot, cType, array_size) ->
 *       cType_fun cType;
 *       array_size_fun array_size
 * 
 *   | PointerToMemberType(annot, atomicType (\* = NamedAtomicType *\), 
 * 			cVFlags, cType) ->
 *       assert(match atomicType with 
 * 	       | SimpleType _ -> false
 * 	       | CompoundType _
 * 	       | PseudoInstantiation _
 * 	       | EnumType _
 * 	       | TypeVariable _ -> true);
 *       atomicType_fun atomicType;
 *       cVFlags_fun cVFlags;
 *       cType_fun cType
 * 
 * 6 and sTemplateArgument_fun = function
 *   | STA_NONE -> ()
 *   | STA_TYPE(cType) -> cType_fun cType
 *   | STA_INT(int) -> int_fun int
 *   | STA_ENUMERATOR(variable) -> variable_fun variable
 *   | STA_REFERENCE(variable) -> variable_fun variable
 *   | STA_POINTER(variable) -> variable_fun variable
 *   | STA_MEMBER(variable) -> variable_fun variable
 *   | STA_DEPEXPR(expression) -> expression_fun expression
 *   | STA_TEMPLATE -> ()
 *   | STA_ATOMIC(atomicType) -> atomicType_fun atomicType
 *)


let count_rev base l =
  let counter = ref (List.length l)
  in
    List.rev_map 
      (fun x -> decr counter; (x, Printf.sprintf "%s[%d]" base !counter)) 
      l

let start_label name color id =
  Printf.fprintf !oc "    \"%d\" [color=\"%s\", label=\"%s %d" 
    id color name id

let finish_label caddr =
  output_string !oc "\"];\n"


let loc_label (file, line, char) =
  ("loc", Printf.sprintf "%s:%d:%d" file line char)

let child_edge id (cid,label) =
  Printf.fprintf !oc "    \"%d\" -> \"%d\" [label=\"%s\"];\n" id cid label

let child_edges id childs = 
  List.iter (child_edge id) childs

  
let opt_child child_fun field_name opt child_list = 
  match opt with
    | None -> child_list
    | Some c -> (child_fun c, field_name) :: child_list

let caddr_label caddr =
  ("caddr", Printf.sprintf "0x%lx" (Int32.shift_left (Int32.of_int caddr) 1))

let ast_node color annot name (labels :(string*string) list) childs =
  let id = id_annotation annot
  in
    start_label name color id;
    List.iter any_label 
      (if !print_caddr then
	 labels @ [(caddr_label (caddr_annotation annot))]
       else
	 labels);
    finish_label ();
    child_edges id childs;
    id

let ast_loc_node color annot loc name labels childs =
  ast_node color annot name ((loc_label loc) :: labels) childs

(***************** colors *****************************)

let color_TranslationUnit = "red"
let color_TF = "firebrick2"
let color_Declaration = "cyan"
let color_Declarator = "SkyBlue"
let color_Function = "magenta"
let color_IDeclarator = "SteelBlue"
let color_Member = "SlateBlue1"
let color_MemberList = "SlateBlue4"
let color_PQName = "DarkViolet"
let color_TypeSpecifier = "OliveDrab"
let color_Statement = "yellow"
let color_handler = "khaki1"
let color_FullExpression = "coral"
let color_Expression = "orange"
let color_ArgExpression = "DarkOrange"
let color_ArgExpressionListOpt = "tomato"
let color_Condition = "OrangeRed"
let color_Initializer = "gold"
let color_ASTTypeID = "MediumAquamarine"
let color_Enumerator = "PaleTurquoise"



(***************** generated ast nodes ****************)

let rec translationUnit_fun 
               ((annot, topForm_list) : annotated translationUnit_type) =
  ast_node color_TranslationUnit annot "TranslationUnit" []
    (count_rev "topForms" (List.rev_map topForm_fun topForm_list))


and topForm_fun tf =
  let tf_node = ast_loc_node color_TF (topForm_annotation tf) (topForm_loc tf) 
  in
  let tf_node_11 name label child = tf_node name [label] [child] 
  in match tf with
    | TF_decl(annot, loc, declaration) -> 
	tf_node "TF_decl" [] [(declaration_fun declaration, "decl")]

    | TF_func(annot, loc, func) -> 
	tf_node "TF_func" [] [(func_fun func, "f")]

    | TF_template(annot, loc, templateDeclaration) -> 
	tf_node "TF_template" []
	  [(templateDeclaration_fun templateDeclaration, "td")]

    | TF_explicitInst(annot, loc, declFlags, declaration) -> 
	tf_node_11 "TF_explicitInst" 
	  ("instFlags", string_of_declFlags declFlags)
	  (declaration_fun declaration, "d")

    | TF_linkage(annot, loc, linkage, translationUnit) -> 
	tf_node_11 "TF_linkage" 
	  ("linkage", linkage)
	  (translationUnit_fun translationUnit, "forms")

    | TF_one_linkage(annot, loc, linkage, topForm) -> 
	tf_node_11 "TF_one_linkage"
	  ("linkage", linkage)
	  (topForm_fun topForm, "form")

    | TF_asm(annot, loc, e_stringLit) -> 
	assert(match e_stringLit with | E_stringLit _ -> true | _ -> false);
	tf_node "TF_asm" [] [(expression_fun e_stringLit, "text")]

    | TF_namespaceDefn(annot, loc, name_opt, topForm_list) -> 
	tf_node "TF_namespaceDefn" 
	  [("name", string_opt name_opt)]
	  (count_rev "forms" (List.rev_map topForm_fun topForm_list))

    | TF_namespaceDecl(annot, loc, namespaceDecl) -> 
	tf_node "TF_namespaceDecl" [] 
	  [(namespaceDecl_fun namespaceDecl, "decl")]


and templateDeclaration_fun _ = not_implemented ()
and namespaceDecl_fun _ = not_implemented ()



and func_fun(annot, declFlags, typeSpecifier, declarator, memberInit_list, 
	 s_compound_opt, handler_list, statement_opt, bool) =
  let _ = assert(match s_compound_opt with
		   | None -> true
		   | Some s_compound ->
		       match s_compound with 
			 | S_compound _ -> true 
			 | _ -> false)
  in
    ast_node color_Function annot "Function"
      [("dflags", string_of_declFlags declFlags);
       ("implicit def", string_of_bool bool)]
      ((typeSpecifier_fun typeSpecifier, "retspec") ::
	 (declarator_fun declarator, "nameAndParams") ::
	 (count_rev "inits" (List.rev_map memberInit_fun memberInit_list)) @
	 (opt_child statement_fun "body" s_compound_opt
	    ((count_rev "handlers" (List.rev_map handler_fun handler_list)) @ 
	       (opt_child statement_fun "dtor" statement_opt []))))


and memberInit_fun _ = not_implemented ()

(* 
 * 
 * 10 and memberInit_fun(annot, pQName, argExpression_list, statement_opt) =
 *   pQName_fun pQName;
 *   List.iter argExpression_fun argExpression_list;
 *   opt_iter statement_fun statement_opt
 * 
 * 
 *)

and declaration_fun(annot, declFlags, typeSpecifier, declarator_list) =
  ast_node color_Declaration annot "Declaration" 
    [("dflags", string_of_declFlags declFlags)]
    ((typeSpecifier_fun typeSpecifier, "spec") ::
       count_rev "decllist" (List.rev_map declarator_fun declarator_list))

and aSTTypeId_fun(annot, typeSpecifier, declarator) =
  ast_node color_ASTTypeID annot "ASTTypeId" []
    [(typeSpecifier_fun typeSpecifier, "spec");
     (declarator_fun declarator, "decl")]


and pQName_fun pq = 
  let pq_node = ast_loc_node color_PQName (pQName_annotation pq) (pQName_loc pq)
  in match pq with
    | PQ_qualifier(annot, loc, stringRef_opt, templateArgument_opt, pQName) -> 
	pq_node "PQ_qualifier" 
	  [("qualifier", string_opt stringRef_opt)]
	  (opt_child templateArgument_fun "templArgs" templateArgument_opt
	     [(pQName_fun pQName, "rest")])

    | PQ_name(annot, loc, stringRef) -> 
	pq_node "PQ_name" [("name", stringRef)] []

    | PQ_operator(annot, loc, operatorName, stringRef) -> 
	pq_node "PQ_operator" 
	  [("fakeName", stringRef)]
	  [(operatorName_fun operatorName, "o")]

    | PQ_template(annot, loc, stringRef, templateArgument_opt) -> 
	pq_node "PQ_template" 
	  [("name", stringRef)]
	  (opt_child templateArgument_fun "templArgs" templateArgument_opt [])

    | PQ_variable(annot, loc, variable) -> 
	pq_node "PQ_variable" [] [(variable_fun variable, "var")]


and templateArgument_fun _ = not_implemented ()
and operatorName_fun _ = not_implemented ()
and variable_fun _ = not_implemented ()

and typeSpecifier_fun ts =
  let tsnode name labels childs = 
    ast_loc_node color_TypeSpecifier (typeSpecifier_annotation ts) 
      (typeSpecifier_loc ts) name
      (("cv", string_of_cVFlags (typeSpecifier_cv ts)) :: labels)
      childs
  in let tsnode_1d name field value childs = tsnode name [(field, value)] childs
  in match ts with
    | TS_name(annot, loc, cVFlags, pQName, bool) -> 
	tsnode_1d "TS_name" "typenameUsed" (string_of_bool bool)
	  [(pQName_fun pQName, "name")]

    | TS_simple(annot, loc, cVFlags, simpleTypeId) -> 
	tsnode_1d "TS_simple" "id" (string_of_simpleTypeId simpleTypeId) []

    | TS_elaborated(annot, loc, cVFlags, typeIntr, pQName) -> 
	tsnode_1d "TS_elaborated" "keyword" (string_of_typeIntr typeIntr)
	  [(pQName_fun pQName, "name")]

    | TS_classSpec(annot, loc, cVFlags, typeIntr, pQName_opt, 
		   baseClassSpec_list, memberList) -> 
	tsnode_1d "TS_classSpec" "keyword" (string_of_typeIntr typeIntr)
	  (opt_child pQName_fun "name" pQName_opt
	     (count_rev "bases" 
		(List.rev_map baseClassSpec_fun baseClassSpec_list)) @
	     [(memberList_fun memberList, "members")])

    | TS_enumSpec(annot, loc, cVFlags, stringRef_opt, enumerator_list) -> 
	tsnode_1d "TS_enumSpec" "name" (string_opt stringRef_opt)
	  (count_rev "elts" (List.rev_map enumerator_fun enumerator_list))

    | TS_type(annot, loc, cVFlags, cType) -> 
	tsnode "TS_type" [] [(cType_fun cType, "type")]

    | TS_typeof(annot, loc, cVFlags, aSTTypeof) -> 
	tsnode "TS_typeof" [] [(aSTTypeof_fun aSTTypeof, "atype")]

and baseClassSpec_fun _ = not_implemented ()
and cType_fun _ = not_implemented ()
and aSTTypeof_fun _ = not_implemented ()

(*
 * 
 * 37 and baseClassSpec_fun(annot, bool, accessKeyword, pQName) =
 *   bool_fun bool;
 *   accessKeyword_fun accessKeyword;
 *   pQName_fun pQName
 *)

and enumerator_fun(annot, loc, stringRef, expression_opt) =
  ast_loc_node color_Enumerator annot loc "Enumerator" 
    [("name", stringRef)]
    (opt_child expression_fun "expr" expression_opt [])


and memberList_fun(annot, member_list) =
  ast_node color_MemberList annot "MemberList" []
    (count_rev "list" (List.rev_map member_fun member_list))

and member_fun m = 
  let mnode = ast_loc_node color_Member (member_annotation m) (member_loc m) in
  let mnode_1c name child field = mnode name [] [(child, field)] 
  in match m with
    | MR_decl(annot, loc, declaration) -> 
	mnode_1c "MR_decl" (declaration_fun declaration) "d"

    | MR_func(annot, loc, func) -> 
	mnode_1c "MR_func" (func_fun func) "f"

    | MR_access(annot, loc, accessKeyword) -> 
	mnode "MR_access" [("k", string_of_accessKeyword accessKeyword)] []

    | MR_usingDecl(annot, loc, nd_usingDecl) -> 
	assert(match nd_usingDecl with ND_usingDecl _ -> true | _ -> false);
	mnode_1c "MR_usingDecl" (namespaceDecl_fun nd_usingDecl) "decl"

    | MR_template(annot, loc, templateDeclaration) -> 
	mnode_1c "MR_template" (templateDeclaration_fun templateDeclaration) "d"


and declarator_fun(annot, iDeclarator, init_opt, 
		   statement_opt_ctor, statement_opt_dtor) =
  ast_node color_Declarator annot "Declarator" []
    ((iDeclarator_fun iDeclarator, "decl") :: 
       (opt_child init_fun "init" init_opt
	  (opt_child statement_fun "ctor" statement_opt_ctor
	     (opt_child statement_fun "dtor" statement_opt_dtor []))))
    

and iDeclarator_fun idecl =
  let inode = ast_loc_node color_IDeclarator 
    (iDeclarator_annotation idecl) (iDeclarator_loc idecl) 
  in let inode_1d name field value childs = inode name [(name,field)] childs
  in match idecl with
    | D_name(annot, loc, pQName_opt) -> 
	inode "D_name" [] (opt_child pQName_fun "name" pQName_opt [])

    | D_pointer(annot, loc, cVFlags, iDeclarator) -> 
	inode_1d "D_pointer" "cv" (string_of_cVFlags cVFlags)
	  [(iDeclarator_fun iDeclarator, "base")]

    | D_reference(annot, loc, iDeclarator) -> 
	inode "D_reference" [] [(iDeclarator_fun iDeclarator, "base")]

    | D_func(annot, loc, iDeclarator, aSTTypeId_list, cVFlags, 
	     exceptionSpec_opt, pq_name_list) -> 
	assert(List.for_all (function | PQ_name _ -> true | _ -> false) 
		 pq_name_list);
	inode_1d "D_func" "cv" (string_of_cVFlags cVFlags)
	  ((iDeclarator_fun iDeclarator, "base") ::
	     (count_rev "params" (List.rev_map aSTTypeId_fun aSTTypeId_list)) @
	     (opt_child exceptionSpec_fun "exnSpec" exceptionSpec_opt
		(count_rev "kAndR_params" 
		   (List.rev_map pQName_fun pq_name_list))))

    | D_array(annot, loc, iDeclarator, expression_opt) -> 
	inode "D_array" [] 
	  ((iDeclarator_fun iDeclarator, "base") ::
	     (opt_child expression_fun "size" expression_opt []))

    | D_bitfield(annot, loc, pQName_opt, expression) -> 
	inode "D_bitfield" []
	  (opt_child pQName_fun "name" pQName_opt
	     [(expression_fun expression, "bits")])

    | D_ptrToMember(annot, loc, pQName, cVFlags, iDeclarator) -> 
	inode_1d "D_ptrToMember" "cv" (string_of_cVFlags cVFlags)
	  [(pQName_fun pQName, "nestedName");
	   (iDeclarator_fun iDeclarator, "base")]

    | D_grouping(annot, loc, iDeclarator) -> 
	inode "D_grouping" [] [(iDeclarator_fun iDeclarator, "base")]


and exceptionSpec_fun _ = not_implemented ()

(* 
 * 17 and exceptionSpec_fun(annot, aSTTypeId_list) =
 *   List.iter aSTTypeId_fun aSTTypeId_list
 * 
 * 
 * 18 and operatorName_fun = function
 *   | ON_newDel(annot, bool_is_new, bool_is_array) -> 
 *       bool_fun bool_is_new;
 *       bool_fun bool_is_array
 * 
 *   | ON_operator(annot, overloadableOp) -> 
 *       overloadableOp_fun overloadableOp
 * 
 *   | ON_conversion(annot, aSTTypeId) -> 
 *       aSTTypeId_fun aSTTypeId
 *)

and statement_fun s =
  let snode = 
    ast_loc_node color_Statement (statement_annotation s) (statement_loc s)
  in let snode_1d name field value childs = snode name [(field, value)] childs
  in match s with
    | S_skip(annot, loc) -> 
	snode "S_skip" [] []

    | S_label(annot, loc, stringRef, statement) -> 
	snode_1d "S_label" "name" stringRef [(statement_fun statement, "s")]

    | S_case(annot, loc, expression, statement) -> 
	snode "S_case" []
	  [(expression_fun expression, "expr");
	   (statement_fun statement, "s")]

    | S_default(annot, loc, statement) -> 
	snode "S_default" [] [(statement_fun statement, "s")]

    | S_expr(annot, loc, fullExpression) -> 
	snode "S_expr" [] [(fullExpression_fun fullExpression, "s")]

    | S_compound(annot, loc, statement_list) -> 
	snode "S_compound" []
	  (count_rev "stmts" (List.rev_map statement_fun statement_list))

    | S_if(annot, loc, condition, statement_then, statement_else) -> 
	snode "S_if" []
	  [(condition_fun condition, "cond");
	   (statement_fun statement_then, "then");
	   (statement_fun statement_else, "else")]

    | S_switch(annot, loc, condition, statement) -> 
	snode "S_switch" []
	  [(condition_fun condition, "cond");
	   (statement_fun statement, "branches")]

    | S_while(annot, loc, condition, statement) -> 
	snode "S_while" []
	  [(condition_fun condition, "cond");
	   (statement_fun statement, "body")]

    | S_doWhile(annot, loc, statement, fullExpression) -> 
	snode "S_doWhile" []
	  [(statement_fun statement, "body");
	   (fullExpression_fun fullExpression, "expr")]

    | S_for(annot, loc, statement_init, condition, 
	    fullExpression, statement_body) -> 
	snode "S_for" []
	  [(statement_fun statement_init, "init");
	   (condition_fun condition, "cond");
	   (fullExpression_fun fullExpression, "after");
	   (statement_fun statement_body, "body")]

    | S_break(annot, loc) -> 
	snode "S_break" [] []

    | S_continue(annot, loc) ->
	snode "S_continue" [] []

    | S_return(annot, loc, fullExpression_opt, statement_opt) -> 
	snode "S_return" []
	  (opt_child fullExpression_fun "expr" fullExpression_opt
	     (opt_child statement_fun "copy_ctor" statement_opt []))

    | S_goto(annot, loc, stringRef) -> 
	snode_1d "S_goto" "target" stringRef []

    | S_decl(annot, loc, declaration) -> 
	snode "S_decl" [] [(declaration_fun declaration, "decl")]

    | S_try(annot, loc, statement, handler_list) -> 
	snode "S_try" []
	  ((statement_fun statement, "body") ::
	     (count_rev "handler" (List.rev_map handler_fun handler_list)))

    | S_asm(annot, loc, e_stringLit) -> 
	assert(match e_stringLit with | E_stringLit _ -> true | _ -> false);
	snode "S_asm" [] [(expression_fun e_stringLit, "text")]

    | S_namespaceDecl(annot, loc, namespaceDecl) -> 
	snode "S_namespaceDecl" [] 
	  [(namespaceDecl_fun namespaceDecl, "decl")]

    | S_function(annot, loc, func) -> 
	snode "S_function" []
	  [(func_fun func, "f")]

    | S_rangeCase(annot, loc, expression_lo, expression_hi, statement) -> 
	snode "S_rangeCase" []
	  [(expression_fun expression_lo, "exprLo");
	   (expression_fun expression_hi, "exprHi");
	   (statement_fun statement, "s")]

    | S_computedGoto(annot, loc, expression) -> 
	snode "S_computedGoto" [] [(expression_fun expression, "target")]


and condition_fun co = 
  let conode = ast_node color_Condition (condition_annotation co)
  in match co with
    | CN_expr(annot, fullExpression) -> 
	conode "CN_expr" [] [(fullExpression_fun fullExpression, "expr")]

    | CN_decl(annot, aSTTypeId) -> 
	conode "CN_decl" [] [(aSTTypeId_fun aSTTypeId, "typeId")]


and handler_fun(annot, aSTTypeId, statement_body,
		expression_opt, statement_gdtor) =
  ast_node color_handler annot "Handler" []
    ((aSTTypeId_fun aSTTypeId, "typeId") ::
       (statement_fun statement_body, "body") ::
       (opt_child expression_fun "localArg" expression_opt
	  (opt_child statement_fun "globalDtor" statement_gdtor [])))


and expression_fun ex = 
  let exnode = ast_node color_Expression (expression_annotation ex) in
  let exnode_1d name field value childs = exnode name [(field, value)] childs
  in match ex with
    | E_boolLit(annot, bool) -> 
	exnode_1d "E_boolLit" "b" (string_of_bool bool) []

    | E_intLit(annot, stringRef) -> 
	exnode_1d "E_intLit" "text" stringRef []

    | E_floatLit(annot, stringRef) -> 
	exnode_1d "E_floatLit" "text" stringRef []

    | E_stringLit(annot, stringRef, e_stringLit_opt) -> 
	assert(match e_stringLit_opt with 
		 | Some(E_stringLit _) -> true 
		 | None -> true
		 | _ -> false);
	exnode_1d "E_stringLit" "text" stringRef
	  (opt_child expression_fun "continuation" e_stringLit_opt [])

    | E_charLit(annot, stringRef) -> 
	exnode_1d "E_charLit" "text" stringRef []

    | E_this annot -> 
	exnode "E_this" [] []

    | E_variable(annot, pQName) -> 
	exnode "E_variable" [] [(pQName_fun pQName, "name")]

    | E_funCall(annot, expression_func, argExpression_list, 
		expression_retobj_opt) -> 
	exnode "E_funCall" []
	  ((expression_fun expression_func, "func") ::
	     (count_rev "args"
		(List.rev_map argExpression_fun argExpression_list)) @
	     (opt_child expression_fun "retObj" expression_retobj_opt []))

    | E_constructor(annot, typeSpecifier, argExpression_list, 
		    bool, expression_opt) -> 
	exnode_1d "E_constructor" "artificial" (string_of_bool bool)
	  ((typeSpecifier_fun typeSpecifier, "spec") ::
	     (count_rev "args" 
		(List.rev_map argExpression_fun argExpression_list)) @
	     (opt_child expression_fun "retObj" expression_opt []))

    | E_fieldAcc(annot, expression, pQName) -> 
	exnode "E_fieldAcc" []
	  [(expression_fun expression, "obj");
	   (pQName_fun pQName, "fieldName")]

    | E_sizeof(annot, expression) -> 
	exnode "E_sizeof" []
	  [(expression_fun expression, "expr")]

    | E_unary(annot, unaryOp, expression) -> 
	exnode_1d "E_unary" "op" (string_of_unaryOp unaryOp)
	  [(expression_fun expression, "expr")]

    | E_effect(annot, effectOp, expression) -> 
	exnode_1d "E_effect" "op" (string_of_effectOp effectOp)
	  [(expression_fun expression, "expr")]

    | E_binary(annot, expression_left, binaryOp, expression_right) -> 
	exnode_1d "E_binary" "op" (string_of_binaryOp binaryOp) 
	  [(expression_fun expression_left, "e1");
	   (expression_fun expression_right, "e2")]

    | E_addrOf(annot, expression) -> 
	exnode "E_addrOf" [] [(expression_fun expression, "expr")]

    | E_deref(annot, expression) -> 
	exnode "E_deref" [] [(expression_fun expression, "prt")]

    | E_cast(annot, aSTTypeId, expression) -> 
	exnode "E_cast" []
	  [
	    (expression_fun expression, "expr");
	    (aSTTypeId_fun aSTTypeId, "ctype");
	  ]

    | E_cond(annot, expression_cond, expression_true, expression_false) -> 
	exnode "E_cond" []
	  [(expression_fun expression_cond, "cond");
	   (expression_fun expression_true, "th");
	   (expression_fun expression_false, "el")]

    | E_sizeofType(annot, aSTTypeId) -> 
	exnode "E_sizeofType" []
	  [(aSTTypeId_fun aSTTypeId, "atype")]

    | E_assign(annot, expression_target, binaryOp, expression_src) -> 
	exnode_1d "E_assign" "op" (string_of_binaryOp binaryOp)
	  [(expression_fun expression_target, "target");
	   (expression_fun expression_src, "src")]

    | E_new(annot, bool, argExpression_list, aSTTypeId, 
	    argExpressionListOpt_opt, statement_opt) -> 
	exnode_1d "E_new" "colonColon" (string_of_bool bool)
	  ((count_rev "placementArgs"
	      (List.rev_map argExpression_fun argExpression_list)) @
	     ((aSTTypeId_fun aSTTypeId, "atype") ::
		(opt_child argExpressionListOpt_fun 
		   "ctorArgs" argExpressionListOpt_opt
		   (opt_child statement_fun "ctorStatement" statement_opt []))))

    | E_delete(annot, bool_colon, bool_array, expression_opt, statement_opt) ->
	exnode "E_delete"
	  [("colonColon", string_of_bool bool_colon);
	   ("array", string_of_bool  bool_array)]
	  (opt_child expression_fun "expr" expression_opt
	     (opt_child statement_fun "dtorStatement" statement_opt []))

    | E_throw(annot, expression_opt, statement_opt) -> 
	exnode "E_throw" []
	  (opt_child expression_fun "expr" expression_opt
	     (opt_child statement_fun "globalCtorStatement" statement_opt []))

    | E_keywordCast(annot, castKeyword, aSTTypeId, expression) -> 
	exnode_1d "E_keywordCast" "key" (string_of_castKeyword castKeyword)
	  [(aSTTypeId_fun aSTTypeId, "ctype");
	   (expression_fun expression, "expr")]

    | E_typeidExpr(annot, expression) -> 
	exnode "E_typeidExpr" [] [(expression_fun expression, "expr")]

    | E_typeidType(annot, aSTTypeId) -> 
	exnode "E_typeidType" [] [(aSTTypeId_fun aSTTypeId, "ttype")]

    | E_grouping(annot, expression) -> 
	exnode "E_grouping" [] [(expression_fun expression, "expr")]

    | E_arrow(annot, expression, pQName) -> 
	exnode "E_arrow" []
	  [(expression_fun expression, "obj");
	   (pQName_fun pQName, "fieldName")]

    | E_statement(annot, s_compound) -> 
	assert(match s_compound with | S_compound _ -> true | _ -> false);
	exnode "E_statement" [] 
	  [(statement_fun s_compound, "s")]

    | E_compoundLit(annot, aSTTypeId, in_compound) -> 
	assert(match in_compound with | IN_compound _ -> true | _ -> false);
	exnode "E_compoundLit" []
	  [(aSTTypeId_fun aSTTypeId, "stype");
	   (init_fun in_compound, "init")]

    | E___builtin_constant_p(annot, loc, expression) -> 
	ast_loc_node color_Expression annot loc "E___builtin_constant_p" []
	  [(expression_fun expression, "expr")]

    | E___builtin_va_arg(annot, loc, expression, aSTTypeId) -> 
	ast_loc_node color_Expression annot loc "E___builtin_va_arg" []
	  [(expression_fun expression, "expr");
	   (aSTTypeId_fun aSTTypeId, "atype")]

    | E_alignofType(annot, aSTTypeId) -> 
	exnode "E_alignofType" []
	  [(aSTTypeId_fun aSTTypeId, "atype")]

    | E_alignofExpr(annot, expression) -> 
	exnode "E_alignofExpr" []
	  [(expression_fun expression, "expr")]

    | E_gnuCond(annot, expression_cond, expression_false) -> 
	exnode "E_gnuCond" []
	  [(expression_fun expression_cond, "cond");
	   (expression_fun expression_false, "el")]

    | E_addrOfLabel(annot, stringRef) -> 
	exnode_1d "E_addrOfLabel" "labelName" stringRef []


and fullExpression_fun(annot, expression_opt) =
  ast_node color_FullExpression annot "FullExpression" []
    (opt_child expression_fun "expr" expression_opt [])


and argExpression_fun(annot, expression) =
  ast_node color_ArgExpression annot "ArgExpression" []
    [(expression_fun expression, "expr")]


and argExpressionListOpt_fun(annot, argExpression_list) =
  ast_node color_ArgExpressionListOpt annot "ArgExpressionListOpt" []
    (count_rev "list" (List.rev_map argExpression_fun argExpression_list))


and init_fun i = 
  let inode = ast_loc_node color_Initializer (init_annotation i) (init_loc i) 
  in match i with
    | IN_expr(annot, loc, expression) -> 
	inode "IN_expr" [] [(expression_fun expression, "e")]

    | IN_compound(annot, loc, init_list) -> 
	inode "IN_compound" []
	  (count_rev "inits" (List.rev_map init_fun init_list))

    | IN_ctor(annot, loc, argExpression_list, bool) -> 
	inode "IN_ctor" 
	  [("was_IN_expr", string_of_bool bool)]
	  (count_rev "args" (List.rev_map argExpression_fun argExpression_list))

    | IN_designated(annot, loc, designator_list, init) -> 
	inode "IN_designated" []
	  ((count_rev "designator"
	      (List.rev_map designator_fun designator_list)) @
	     [(init_fun init, "init")])

and designator_fun _ = not_implemented ()

(* 27 and templateDeclaration_fun = function
 *   | TD_func(annot, templateParameter_opt, func) -> 
 *       opt_iter templateParameter_fun templateParameter_opt;
 *       func_fun func
 * 
 *   | TD_decl(annot, templateParameter_opt, declaration) -> 
 *       opt_iter templateParameter_fun templateParameter_opt;
 *       declaration_fun declaration
 * 
 *   | TD_tmember(annot, templateParameter_opt, templateDeclaration) -> 
 *       opt_iter templateParameter_fun templateParameter_opt;
 *       templateDeclaration_fun templateDeclaration
 * 
 * 
 * 28 and templateParameter_fun = function
 *   | TP_type(annot, loc, stringRef, aSTTypeId_opt, templateParameter_opt) -> 
 *       string_fun stringRef;
 *       opt_iter aSTTypeId_fun aSTTypeId_opt;
 *       opt_iter templateParameter_fun templateParameter_opt
 * 
 *   | TP_nontype(annot, loc, aSTTypeId, templateParameter_opt) -> 
 *       aSTTypeId_fun aSTTypeId;
 *       opt_iter templateParameter_fun templateParameter_opt
 * 
 * 
 * 29 and templateArgument_fun = function
 *   | TA_type(annot, aSTTypeId, templateArgument_opt) -> 
 *       aSTTypeId_fun aSTTypeId;
 *       opt_iter templateArgument_fun templateArgument_opt
 * 
 *   | TA_nontype(annot, expression, templateArgument_opt) -> 
 *       expression_fun expression;
 *       opt_iter templateArgument_fun templateArgument_opt
 * 
 *   | TA_templateUsed(annot, templateArgument_opt) -> 
 *       opt_iter templateArgument_fun templateArgument_opt
 * 
 * 
 * 30 and namespaceDecl_fun = function
 *   | ND_alias(annot, stringRef, pQName) -> 
 *       string_fun stringRef;
 *       pQName_fun pQName
 * 
 *   | ND_usingDecl(annot, pQName) -> 
 *       pQName_fun pQName
 * 
 *   | ND_usingDir(annot, pQName) -> 
 *       pQName_fun pQName
 * 
 * 
 * 31 and fullExpressionAnnot_fun(declaration_list) =
 *     List.iter declaration_fun declaration_list
 * 
 * 
 * 32 and aSTTypeof_fun = function
 *   | TS_typeof_expr(annot, fullExpression) -> 
 *       fullExpression_fun fullExpression
 * 
 *   | TS_typeof_type(annot, aSTTypeId) -> 
 *       aSTTypeId_fun aSTTypeId
 * 
 * 
 * 33 and designator_fun = function
 *   | FieldDesignator(annot, loc, stringRef) -> 
 *       string_fun stringRef
 * 
 *   | SubscriptDesignator(annot, loc, expression, expression_opt) -> 
 *       expression_fun expression;
 *       opt_iter expression_fun expression_opt
 * 
 * 
 * 34 and attributeSpecifierList_fun = function
 *   | AttributeSpecifierList_cons(annot, attributeSpecifier, 
 * 				attributeSpecifierList) -> 
 *       attributeSpecifier_fun attributeSpecifier;
 *       attributeSpecifierList_fun 
 * 	attributeSpecifierList
 * 
 * 
 * 35 and attributeSpecifier_fun = function
 *   | AttributeSpecifier_cons(annot, attribute, attributeSpecifier) -> 
 *       attribute_fun attribute;
 *       attributeSpecifier_fun attributeSpecifier
 * 
 * 
 * 36 and attribute_fun = function
 *   | AT_empty(annot, loc) -> 
 * 
 *   | AT_word(annot, loc, stringRef) -> 
 *       string_fun stringRef
 * 
 *   | AT_func(annot, loc, stringRef, argExpression_list) -> 
 *       string_fun stringRef;
 *       List.iter argExpression_fun argExpression_list
 *)


(**************************************************************************
 *
 * end of astiter.ml 
 *
 **************************************************************************)


let out_file = ref ""


let arguments = Arg.align
  [
    ("-o", Arg.Set_string out_file,
     "file set output file name");
  ]

let usage_msg = 
  "usage: ast_graph [options...] <file>\n\
   recognized options are:"

let usage () =
  prerr_endline usage_msg;
  exit(1)
  
let file = ref ""

let file_set = ref false

let anonfun fn = 
  if !file_set 
  then
    begin
      Printf.eprintf "don't know what to do with %s\n" fn;
      usage()
    end
  else
    begin
      file := fn;
      file_set := true
    end

let start_file infile =
  output_string !oc "digraph ";
  Printf.fprintf !oc "\"%s\"" infile;
  output_string !oc " {\n";
  output_string !oc 
    "    color=white    node [ color = grey95, style = filled ]\n"

let finish_file () =
  output_string !oc "}\n"

let main () =
  Arg.parse arguments anonfun usage_msg;
  if not !file_set then
    usage();				(* does not return *)
  let ic = open_in !file in
  let ofile = 
    if !out_file <> "" 
    then !out_file
    else "nodes.dot"
  in
  let _ = oc := open_out (ofile) in
  let ast = (Marshal.from_channel ic : annotated translationUnit_type)
  in
    start_file !file;
    ignore(translationUnit_fun ast);
    finish_file ()
      
;;


Printexc.catch main ()


