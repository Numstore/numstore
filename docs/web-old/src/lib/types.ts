export const scalarOptions = ['U32', 'I32'] as const
export type ScalarName = (typeof scalarOptions)[number]

export interface ScalarType {
  kind: 'scalar'
  name: ScalarName
}

export interface Field {
  key: string
  type: Type // recursion!
}

export interface StructType {
  kind: 'struct'
  fields: Field[]
}

export interface UnionType {
  kind: 'union'
  variants: Field[]
}

export interface EnumType {
  kind: 'enum'
  options: string[]
}

export type Type = StructType | UnionType | EnumType | ScalarType
